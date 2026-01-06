// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#include "codegen.h"

#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "codegen_types.h"
#include "common.h"
#include "codegen_emitter_runtime.h"
#include "semantics.h"
#include "string_table.h"
#include "type_system.h"

// Forward declarations
class CodeGen;
class CodeGen {
public:
  CodeGen(const Program &program) : program_(program), string_table_(4096) {}

  std::string Generate() {
    std::cerr << "Init Structs..." << std::endl;
    InitStructs(program_, structs_);
    std::cerr << "Compute Layouts..." << std::endl;
    ComputeStructLayouts(program_, structs_);
    std::cerr << "Build Catalog..." << std::endl;
    BuildFunctionCatalog(program_, structs_, functions_);
    std::cerr << "Build Strings..." << std::endl;
    string_table_.Build(program_);
    std::cerr << "Type Check..." << std::endl;

    // Type Check
    TypeContext ctx{structs_, [this](const std::string &name) {
                      auto it = functions_.find(name);
                      return it == functions_.end() ? nullptr : &it->second;
                    }};

    for (const auto &fn : program_.functions) {
      std::cerr << "Checking function: " << fn.name << std::endl;
      Env env;
      for (auto &p : fn.params) {
        env.params[p.second] =
            LocalInfo{p.second, ResolveType(p.first, structs_)};
        env.locals[p.second] = env.params[p.second];
      }
      CheckStmts(fn.body, env, ctx);
    }
    for (const auto &def : program_.structs) {
      for (const auto &method : def.methods) {
        std::cerr << "Checking method: " << def.name << "." << method.name
                  << std::endl;
        Env env;
        env.current_struct = def.name;
        // Add 'this'
        env.params["this"] = LocalInfo{
            "this", ResolveType(TypeSpec{def.name, 0, false}, structs_)};
        for (auto &p : method.params) {
          env.params[p.second] =
              LocalInfo{p.second, ResolveType(p.first, structs_)};
          env.locals[p.second] = env.params[p.second];
        }
        CheckStmts(method.body, env, ctx);
        std::cerr << "Finished method: " << def.name << "." << method.name
                  << std::endl;
      }
    }
    std::cerr << "Finished Type Check Phase" << std::endl;

    // Emit
    std::cerr << "Emit Module Start" << std::endl;
    out_ << "(module\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"fd_write\" (func $fd_write "
            "(param i32 i32 i32 i32) (result i32)))\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"args_sizes_get\" (func "
            "$args_sizes_get (param i32 i32) (result i32)))\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"args_get\" (func $args_get "
            "(param i32 i32) (result i32)))\n";
    out_ << "  (memory (export \"memory\") 128)\n";
    out_ << "  (global $heap (mut i64) (i64.const " << string_table_.HeapStart()
         << "))\n";

    std::cerr << "Emit Data Segments Start" << std::endl;
    EmitDataSegments();
    std::cerr << "Emit Runtime Start" << std::endl;
    EmitRuntime(out_, string_table_.Offsets());

    // Emit Functions
    std::cerr << "Emit Functions Start" << std::endl;
    for (const auto &fn : functions_) {
      std::cerr << "Checking function: " << fn.first << std::endl;
      if (fn.second.decl && !fn.second.decl->is_method) {
        EmitFunctionDescriptor(fn.second, "");
      }
    }
    // Emit Methods
    for (auto &def : program_.structs) {
      for (auto &m : def.methods) {
        std::string fullname = def.name + "." + m.name;
        if (functions_.count(fullname)) {
          EmitFunctionDescriptor(functions_.at(fullname), def.name);
        }
      }
    }

    // Struct Inits
    for (const auto &def : program_.structs) {
      EmitStructInit(def);
    }

    EmitStart();
    out_ << ")\n";
    return out_.str();
  }

private:
  const Program &program_;
  std::unordered_map<std::string, StructInfo> structs_;
  std::unordered_map<std::string, FunctionInfo> functions_;
  StringLiteralTable string_table_;
  std::ostringstream out_;

  void EmitDataSegments() {
    for (const auto &seg : string_table_.Segments()) {
      out_ << "  (data (i32.const " << seg.first << ") \""
           << EscapeBytes(seg.second) << "\")\n";
    }
  }

  std::string EscapeBytes(const std::string &bytes) {
    std::ostringstream ss;
    for (unsigned char c : bytes) {
      if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
        ss << c;
      } else {
        ss << "\\";
        const char *hex = "0123456789ABCDEF";
        ss << hex[(c >> 4) & 0xF] << hex[c & 0xF];
      }
    }
    return ss.str();
  }

  bool NeedsFormatLiteral(const std::string &text) const {
    for (char c : text) {
      if (c == '%' || c == '\n')
        return true;
    }
    return false;
  }

  void EmitFunctionDescriptor(const FunctionInfo &info,
                              const std::string &owner) {
    std::cerr << "EmitFunctionDescriptor: " << info.wasm_name
              << " decl=" << info.decl << std::endl;
    out_ << "  (func " << info.wasm_name;
    if (owner.empty()) {
      if (info.decl) {
        std::cerr << "Exporting: " << info.decl->name << std::endl;
        out_ << " (export \"" << info.decl->name << "\")";
      }
    }

    std::cerr << "Emit params, count=" << info.params.size() << std::endl;
    Env env;
    std::cerr << "Env created" << std::endl;
    env.current_struct = owner;
    std::cerr << "Owner set" << std::endl;

    int param_idx = 0;
    for (const auto &p : info.params) {
      std::string pname = "$p" + std::to_string(param_idx++);
      if (owner.empty() == false && param_idx == 1)
        pname = "$this"; // this is first param

      out_ << " (param " << pname << " " << WasmType(p) << ")";

      // Register in env
      // We need to map back to original names?
      // The Info params are just types.
      // We need to look up names from decl.

      std::string name;
      if (!owner.empty() && param_idx == 1) {
        name = "this";
      } else {
        int p_index_in_decl = param_idx - 1;
        if (!owner.empty())
          p_index_in_decl--; // adjust for this
        if (info.decl && p_index_in_decl >= 0 &&
            p_index_in_decl < (int)info.decl->params.size()) {
          name = info.decl->params[p_index_in_decl].second;
        } else {
          name = "param_" + std::to_string(param_idx); // Should not happen
        }
      }
      env.params[name] = LocalInfo{pname, p};
      env.locals[name] = LocalInfo{pname, p};
    }

    std::cerr << "Checking return type: " << info.return_type << std::endl;
    if (info.return_type && info.return_type->kind != TypeKind::Void) {
      out_ << " (result " << WasmType(info.return_type) << ")";
    }

    // Locals
    std::vector<LocalInfo> locals;
    if (info.decl) {
      CollectLocals(info.decl->body, env, locals);
    }

    out_ << " (local $tmp0 i64) (local $tmp1 i64) (local $tmp2 i64) (local "
            "$tmp3 i64) (local $tmp4 i64) (local $tmpf f64)";
    for (const auto &l : locals) {
      out_ << " (local " << l.wasm_name << " " << WasmType(l.type) << ")";
    }
    out_ << "\n";

    if (info.decl) {
      EmitStmts(info.decl->body, env);
    }

    if (info.return_type->kind != TypeKind::Void) {
      // If not returned, emit zero (safe default)
      // But usually control flow handles return.
      EmitZero(info.return_type);
    } else {
      out_ << "    (nop)\n";
    }
    out_ << "  )\n";
  }

  void CollectLocals(const std::vector<StmtPtr> &stmts, Env &env,
                     std::vector<LocalInfo> &locals) {
    for (const auto &s : stmts)
      CollectLocals(s, env, locals);
  }

  void CollectLocals(const StmtPtr &stmt, Env &env,
                     std::vector<LocalInfo> &locals) {
    if (!stmt)
      return;
    if (stmt->kind == StmtKind::VarDecl) {
      std::cerr << "CollectLocals VarDecl: " << stmt->var_name
                << " type=" << stmt->var_type.name << std::endl;
      LocalInfo local;
      local.type = ResolveType(stmt->var_type, structs_);
      std::cerr << "  Resolved type addr: " << local.type.get() << std::endl;
      local.wasm_name = "$v" + stmt->var_name;
      env.locals[stmt->var_name] = local;
      locals.push_back(local);
    } else if (stmt->kind == StmtKind::If) {
      CollectLocals(stmt->then_body, env, locals);
      CollectLocals(stmt->else_body, env, locals);
    } else if (stmt->kind == StmtKind::While) {
      CollectLocals(stmt->body, env, locals);
    }
  }

  std::string WasmType(const std::shared_ptr<Type> &type) {
    if (!type)
      return "i64"; // Safety fallback
    std::cerr << "WasmType check: " << type.get() << std::endl;
    if (type->kind == TypeKind::Real)
      return "f64";
    return "i64";
  }

  void EmitZero(const std::shared_ptr<Type> &type) {
    if (type->kind == TypeKind::Real)
      out_ << "    f64.const 0\n";
    else if (type->kind == TypeKind::String)
      out_ << "    i64.const " << string_table_.Offsets().at("") << "\n";
    else
      out_ << "    i64.const 0\n";
  }

  void EmitStmts(const std::vector<StmtPtr> &stmts, Env &env) {
    for (const auto &s : stmts)
      EmitStmt(s, env);
  }

  void EmitStmt(const StmtPtr &stmt, Env &env) {
    if (!stmt)
      return;
    switch (stmt->kind) {
    case StmtKind::VarDecl:
      if (stmt->expr) {
        EmitExpr(stmt->expr, env);
        // implicit cast check not needed as we checked types
        out_ << "    local.set " << env.locals[stmt->var_name].wasm_name
             << "\n";
      } else {
        EmitZero(env.locals[stmt->var_name].type);
        out_ << "    local.set " << env.locals[stmt->var_name].wasm_name
             << "\n";
      }
      break;
    case StmtKind::Assign:
      EmitAssignment(stmt->target, stmt->expr, env);
      break;
    case StmtKind::ExprStmt: {
      auto type = EmitExpr(stmt->expr, env);
      if (type && type->kind != TypeKind::Void) {
        out_ << "    drop\n";
      }
    } break;
    case StmtKind::Return:
      if (stmt->expr) {
        EmitExpr(stmt->expr, env);
      }
      out_ << "    return\n";
      break;
    case StmtKind::If:
      EmitExpr(stmt->expr, env);
      out_ << "    i32.wrap_i64\n    if\n";
      EmitStmts(stmt->then_body, env);
      if (!stmt->else_body.empty()) {
        out_ << "    else\n";
        EmitStmts(stmt->else_body, env);
      }
      out_ << "    end\n";
      break;
    case StmtKind::While:
      out_ << "    block\n      loop\n";
      EmitExpr(stmt->expr, env);
      out_ << "      i32.wrap_i64\n      i32.eqz\n      br_if 1\n";
      EmitStmts(stmt->body, env);
      out_ << "      br 0\n      end\n    end\n";
      break;
    }
  }

  std::shared_ptr<Type> EmitExpr(const ExprPtr &expr, Env &env) {
    if (!expr)
      return nullptr;
    // If generic helpers are available
    if (expr->kind == ExprKind::IntLit) {
      out_ << "    i64.const " << expr->int_value << "\n";
      return ResolveType(TypeSpec{"int", 0, false}, structs_);
    }
    if (expr->kind == ExprKind::RealLit) {
      out_ << "    f64.const " << expr->real_value << "\n";
      return ResolveType(TypeSpec{"real", 0, false}, structs_);
    }
    if (expr->kind == ExprKind::BoolLit) {
      out_ << "    i64.const " << (expr->bool_value ? 1 : 0) << "\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    }
    if (expr->kind == ExprKind::StringLit) {
      out_ << "    i64.const " << string_table_.Offsets().at(expr->text)
           << "\n";
      return ResolveType(TypeSpec{"string", 0, false}, structs_);
    }
    if (expr->kind == ExprKind::Var) {
      return EmitVar(expr, env);
    }
    if (expr->kind == ExprKind::Binary) {
      return EmitBinary(expr, env);
    }
    if (expr->kind == ExprKind::Unary) {
      return EmitUnary(expr, env);
    }
    if (expr->kind == ExprKind::Call) {
      return EmitCall(expr, env);
    }
    if (expr->kind == ExprKind::Field) {
      return EmitField(expr, env);
    }
    if (expr->kind == ExprKind::Index) {
      return EmitIndex(expr, env);
    }
    if (expr->kind == ExprKind::NewExpr) {
      return EmitNew(expr, env);
    }
    return nullptr;
  }

  std::shared_ptr<Type> EmitVar(const ExprPtr &expr, Env &env) {
    auto res = FindIdentifier(expr->text, env, structs_);
    if (!res)
      return nullptr; // Should have been checked
    if (res->kind == LookupResult::Kind::Local ||
        res->kind == LookupResult::Kind::Param) {
      out_ << "    local.get " << res->local->wasm_name << "\n";
      return res->local->type;
    }
    if (res->kind == LookupResult::Kind::Field) {
      out_ << "    local.get $this\n";
      out_ << "    i64.const " << res->field->offset << "\n";
      out_ << "    i64.add\n";
      EmitLoad(res->field->type);
      return res->field->type;
    }
    return nullptr;
  }

  std::shared_ptr<Type> EmitBinary(const ExprPtr &expr, Env &env) {
    auto left = EmitExpr(expr->left, env);
    // Conversion logic if needed
    // For simplicity assuming strict types or simple auto-casting if
    // implemented in binary emitter The original code had auto-cast from int to
    // float
    bool left_int = (left->kind == TypeKind::Int);
    if (left_int && expr->right->type &&
        expr->right->type->kind == TypeKind::Real) {
      out_ << "    f64.convert_i64_s\n";
      EmitExpr(expr->right, env);
      // Op
    } else {
      // Normal emit right
      auto right = EmitExpr(expr->right, env);
      // check if we need to convert right
      if (left->kind == TypeKind::Real && right->kind == TypeKind::Int) {
        out_ << "    f64.convert_i64_s\n";
      }
    }

    // Emit Op
    std::string op = expr->op;
    bool is_float =
        (left->kind == TypeKind::Real ||
         (expr->right->type && expr->right->type->kind == TypeKind::Real));

    if (op == "+")
      out_ << (is_float ? "    f64.add\n" : "    i64.add\n");
    else if (op == "-")
      out_ << (is_float ? "    f64.sub\n" : "    i64.sub\n");
    else if (op == "*")
      out_ << (is_float ? "    f64.mul\n" : "    i64.mul\n");
    else if (op == "/")
      out_ << (is_float ? "    f64.div\n" : "    i64.div_s\n");

    // Relational
    if (op == "==") {
      out_ << (is_float ? "    f64.eq\n" : "    i64.eq\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    }
    if (op == "!=") {
      out_ << (is_float ? "    f64.ne\n" : "    i64.ne\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == "<") {
      out_ << (is_float ? "    f64.lt\n" : "    i64.lt_s\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == "<=") {
      out_ << (is_float ? "    f64.le\n" : "    i64.le_s\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == ">") {
      out_ << (is_float ? "    f64.gt\n" : "    i64.gt_s\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == ">=") {
      out_ << (is_float ? "    f64.ge\n" : "    i64.ge_s\n");
      out_ << "    i64.extend_i32_u\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == "and") {
      out_ << "    i64.and\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == "or") {
      out_ << "    i64.or\n";
      return ResolveType(TypeSpec{"bool", 0, false}, structs_);
    } else if (op == "%") {
      // Modulo, only int?
      out_ << "    i64.rem_s\n";
    }

    if (is_float)
      return ResolveType(TypeSpec{"real", 0, false}, structs_);
    return left;
  }

  std::shared_ptr<Type> EmitUnary(const ExprPtr &expr, Env &env) {
    auto type = EmitExpr(expr->left, env);
    if (expr->op == "-") {
      if (type->kind == TypeKind::Real)
        out_ << "    f64.neg\n";
      else {
        out_ << "    i64.const -1\n    i64.mul\n";
      }
    } else if (expr->op == "!") {
      out_ << "    i64.eqz\n    i64.extend_i32_u\n";
    }
    return type;
  }

  std::shared_ptr<Type> EmitCall(const ExprPtr &expr, Env &env) {
    if (!expr->base)
      return nullptr;
    if (expr->base->kind == ExprKind::Var) {
      std::string name = expr->base->text;
      if (name == "print") {
        if (expr->args.empty()) {
          return ResolveType(TypeSpec{"void", 0, true}, structs_);
        }
        if (expr->args.size() == 1) {
          const auto &arg = expr->args[0];
          auto type = EmitExpr(arg, env);
          if (type->kind == TypeKind::String) {
            if (arg->kind == ExprKind::StringLit &&
                NeedsFormatLiteral(arg->text)) {
              out_ << "    local.set $tmp3\n";
              out_ << "    local.get $tmp3\n";
              out_ << "    i64.const 0\n";
              out_ << "    i32.const 0\n";
              out_ << "    call $print_format\n";
            } else {
              out_ << "    call $print_string\n";
            }
            return ResolveType(TypeSpec{"void", 0, true}, structs_);
          }
          if (type->kind == TypeKind::Int)
            out_ << "    call $print_i64\n";
          else if (type->kind == TypeKind::Bool)
            out_ << "    call $print_bool\n";
          else if (type->kind == TypeKind::Real)
            out_ << "    call $print_f64\n";
          else if (type->kind == TypeKind::String)
            out_ << "    call $print_string\n";
          return ResolveType(TypeSpec{"void", 0, true}, structs_);
        }
        const auto &fmt = expr->args[0];
        EmitExpr(fmt, env);
        out_ << "    local.set $tmp3\n";
        int arg_count = static_cast<int>(expr->args.size()) - 1;
        if (arg_count > 0) {
          out_ << "    i64.const " << (arg_count * 8) << "\n";
          out_ << "    call $alloc\n";
          out_ << "    local.set $tmp4\n";
          for (int i = 0; i < arg_count; ++i) {
            out_ << "    local.get $tmp4\n";
            out_ << "    i64.const " << (i * 8) << "\n";
            out_ << "    i64.add\n";
            out_ << "    local.set $tmp2\n";
            auto type = EmitExpr(expr->args[i + 1], env);
            EmitStore(type);
          }
          out_ << "    local.get $tmp3\n";
          out_ << "    local.get $tmp4\n";
          out_ << "    i32.const " << arg_count << "\n";
          out_ << "    call $print_format\n";
        } else {
          out_ << "    local.get $tmp3\n";
          out_ << "    i64.const 0\n";
          out_ << "    i32.const 0\n";
          out_ << "    call $print_format\n";
        }
        return ResolveType(TypeSpec{"void", 0, true}, structs_);
      }
      if (name == "sqrt") {
        auto type = EmitExpr(expr->args[0], env);
        if (type->kind == TypeKind::Int)
          out_ << "    f64.convert_i64_s\n";
        out_ << "    f64.sqrt\n";
        return ResolveType(TypeSpec{"real", 0, false}, structs_);
      }
      auto it = functions_.find(name);
      if (it != functions_.end()) {
        for (auto &a : expr->args)
          EmitExpr(a, env);
        out_ << "    call " << it->second.wasm_name << "\n";
        return it->second.return_type;
      }
      return nullptr;
    }
    if (expr->base->kind == ExprKind::Field) {
      auto field = expr->base;
      auto base_type = EmitExpr(field->base, env);
      if ((base_type->kind == TypeKind::Array ||
           base_type->kind == TypeKind::String) &&
          field->field == "length") {
        out_ << "    i32.wrap_i64\n";
        out_ << "    i64.load\n";
        return ResolveType(TypeSpec{"int", 0, false}, structs_);
      }
      if (base_type->kind != TypeKind::Struct)
        return nullptr;
      std::string method_name = base_type->name + "." + field->field;
      auto it = functions_.find(method_name);
      if (it == functions_.end())
        return nullptr;
      for (auto &a : expr->args)
        EmitExpr(a, env);
      out_ << "    call " << it->second.wasm_name << "\n";
      return it->second.return_type;
    }
    return nullptr;
  }

  std::shared_ptr<Type> EmitField(const ExprPtr &expr, Env &env) {
    auto base = EmitExpr(expr->base, env);
    auto fit = structs_[base->name].field_map.find(expr->field);
    out_ << "    i64.const " << fit->second.offset << "\n    i64.add\n";
    EmitLoad(fit->second.type);
    return fit->second.type;
  }

  std::shared_ptr<Type> EmitIndex(const ExprPtr &expr, Env &env) {
    auto type = EmitAddress(expr, env);
    EmitLoad(type);
    return type;
  }

  std::shared_ptr<Type> EmitAddress(const ExprPtr &expr, Env &env) {
    if (expr->kind == ExprKind::Field) {
      auto base = EmitExpr(expr->base, env);
      auto &info = structs_.at(base->name);
      auto &field = info.field_map.at(expr->field);
      out_ << "    i64.const " << field.offset << "\n    i64.add\n";
      return field.type;
    }
    if (expr->kind == ExprKind::Index) {
      auto base = EmitExpr(expr->base, env);
      out_ << "    local.set $tmp0\n";
      auto idx_type = EmitExpr(expr->left, env);
      out_ << "    local.set $tmp1\n";

      // base + 8 + idx * size
      int64_t size = GetTypeSize(base->element);
      out_ << "    local.get $tmp0\n";
      out_ << "    i64.const 8\n    i64.add\n";
      out_ << "    local.get $tmp1\n";
      out_ << "    i64.const " << size << "\n    i64.mul\n    i64.add\n";
      return base->element;
    }
    return nullptr;
  }

  std::shared_ptr<Type> EmitNew(const ExprPtr &expr, Env &env) {
    if (expr->new_size) {
      // Array
      auto base = ResolveType(expr->new_type, structs_);
      auto type = std::make_shared<Type>(Type{TypeKind::Array, "", base});

      EmitExpr(expr->new_size, env);
      out_ << "    local.set $tmp0\n"; // size count

      int64_t elem_size = GetTypeSize(base);
      out_ << "    local.get $tmp0\n";
      out_ << "    i64.const " << elem_size << "\n    i64.mul\n";
      out_ << "    i64.const 8\n    i64.add\n";
      out_ << "    call $alloc\n";
      out_ << "    local.set $tmp1\n";

      // store size
      out_ << "    local.get $tmp1\n    i32.wrap_i64\n    local.get $tmp0\n    "
              "i64.store\n";

      out_ << "    local.get $tmp1\n";
      return type;
    }
    // Struct
    auto type = ResolveType(expr->new_type, structs_);
    int64_t size = structs_.at(type->name).size;
    out_ << "    i64.const " << size << "\n    call $alloc\n";
    out_ << "    local.set $tmp0\n";
    out_ << "    local.get $tmp0\n    call $init_" << type->name << "\n";
    out_ << "    local.get $tmp0\n";
    return type;
  }

  void EmitAssignment(const ExprPtr &target, const ExprPtr &value, Env &env) {
    if (target->kind == ExprKind::Var) {
      auto res = FindIdentifier(target->text, env, structs_);
      if (res->kind == LookupResult::Kind::Local ||
          res->kind == LookupResult::Kind::Param) {
        EmitExpr(value, env);
        out_ << "    local.set " << res->local->wasm_name << "\n";
        return;
      }
      if (res->kind == LookupResult::Kind::Field) {
        out_ << "    local.get $this\n";
        out_ << "    i64.const " << res->field->offset << "\n    i64.add\n";
        out_ << "    local.set $tmp2\n";
        auto type = EmitExpr(value, env);
        EmitStore(type);
        return;
      }
    }
    EmitAddress(target, env);
    out_ << "    local.set $tmp2\n";
    auto type = EmitExpr(value, env);
    EmitStore(type);
  }

  void EmitStore(const std::shared_ptr<Type> &type) {
    if (type->kind == TypeKind::Real) {
      out_ << "    local.set $tmpf\n    local.get $tmp2\n    i32.wrap_i64\n    "
              "local.get $tmpf\n    f64.store\n";
    } else {
      out_ << "    local.set $tmp1\n    local.get $tmp2\n    i32.wrap_i64\n    "
              "local.get $tmp1\n    i64.store\n";
    }
  }

  void EmitLoad(const std::shared_ptr<Type> &type) {
    out_ << "    i32.wrap_i64\n";
    if (type->kind == TypeKind::Real)
      out_ << "    f64.load\n";
    else
      out_ << "    i64.load\n";
  }

  void EmitStructInit(const StructDef &def) {
    const auto &info = structs_.at(def.name);
    out_ << "  (func $init_" << def.name
         << " (param $ptr i64) (local $tmp1 i64) (local $tmp2 i64) (local "
            "$tmpf f64)\n";
    for (const auto &field : info.fields) {
      out_ << "    local.get $ptr\n";
      out_ << "    i64.const " << field.offset << "\n";
      out_ << "    i64.add\n";
      out_ << "    local.set $tmp2\n";
      if (field.type->kind == TypeKind::Struct) {
        int64_t size = structs_.at(field.type->name).size;
        out_ << "    i64.const " << size << "\n";
        out_ << "    call $alloc\n";
        out_ << "    local.set $tmp1\n";
        out_ << "    local.get $tmp2\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    local.get $tmp1\n";
        out_ << "    i64.store\n";
        out_ << "    local.get $tmp1\n";
        out_ << "    call $init_" << field.type->name << "\n";
      } else if (field.type->kind == TypeKind::Real) {
        out_ << "    f64.const 0\n";
        out_ << "    local.set $tmpf\n";
        out_ << "    local.get $tmp2\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    local.get $tmpf\n";
        out_ << "    f64.store\n";
      } else if (field.type->kind == TypeKind::String) {
        out_ << "    local.get $tmp2\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    i64.const " << string_table_.Offsets().at("") << "\n";
        out_ << "    i64.store\n";
      } else {
        out_ << "    local.get $tmp2\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    i64.const 0\n";
        out_ << "    i64.store\n";
      }
    }
    out_ << "  )\n";
  }

  void EmitStart() {
    auto it = functions_.find("main");
    if (it != functions_.end()) {
      const auto &info = it->second;
      bool needs_args = false;
      if (info.params.size() == 1 && info.params[0] &&
          info.params[0]->kind == TypeKind::Array &&
          info.params[0]->element &&
          info.params[0]->element->kind == TypeKind::String) {
        needs_args = true;
      }
      out_ << "  (func $_start (export \"_start\")\n";
      if (needs_args) {
        out_ << "    call $build_args\n";
      }
      out_ << "    call " << info.wasm_name << "\n";
      if (info.return_type && info.return_type->kind != TypeKind::Void) {
        out_ << "    drop\n";
      }
      out_ << "  )\n";
    }
  }
};

std::string GenerateWasm(const Program &program,
                         const std::unordered_set<std::string> &type_names) {
  (void)type_names;
  CodeGen cg(program);
  return cg.Generate();
}
