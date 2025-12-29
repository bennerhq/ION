// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codegen.h"
#include "codegen_types.h"
#include "runtime_emitter.h"
#include "struct_layout.h"
#include "type_resolver.h"


class CodeGen {
public:
    CodeGen(const Program &program, const std::unordered_set<std::string> &type_names)
        : program_(program) {
        (void)type_names;
    }

    std::string Generate() {
        resolver_ = std::make_unique<TypeResolver>(program_, structs_);
        resolver_->InitStructs();
        StructLayout layout(program_, *resolver_, structs_);
        layout.ComputeLayouts();
        BuildFunctionTable();
        BuildStringLiterals();
        EmitModule();
        return out_.str();
    }

private:
    const Program &program_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, int64_t> string_offsets_;
    std::vector<std::pair<int64_t, std::string>> data_segments_;
    std::ostringstream out_;
    int64_t data_cursor_ = 4096;
    int64_t heap_start_ = 4096;
    std::unique_ptr<TypeResolver> resolver_;

    struct LocalInfo {
        std::string wasm_name;
        std::shared_ptr<Type> type;
    };

    struct Env {
        std::unordered_map<std::string, LocalInfo> locals;
        std::unordered_map<std::string, LocalInfo> params;
        std::string current_struct;
    };


    std::shared_ptr<Type> ResolveType(const TypeSpec &spec) {
        return resolver_->Resolve(spec);
    }

    int64_t TypeSize(const std::shared_ptr<Type> &type) const {
        switch (type->kind) {
            case Type::Kind::Int:
            case Type::Kind::Real:
            case Type::Kind::Bool:
            case Type::Kind::String:
            case Type::Kind::Struct:
            case Type::Kind::Array:
                return 8;
            case Type::Kind::Void:
                return 0;
        }
        return 8;
    }


    void BuildFunctionTable() {
        for (const auto &fn : program_.functions) {
            FunctionInfo info;
            info.decl = const_cast<Function *>(&fn);
            info.return_type = ResolveType(fn.return_type);
            for (const auto &param : fn.params) {
                info.params.push_back(ResolveType(param.first));
            }
            info.wasm_name = MangleFunctionName(fn, "");
            functions_[fn.name] = info;
        }
        for (const auto &def : program_.structs) {
            for (const auto &method : def.methods) {
                FunctionInfo info;
                info.decl = const_cast<Function *>(&method);
                info.return_type = ResolveType(method.return_type);
                info.params.push_back(ResolveType(TypeSpec{def.name, 0, false}));
                for (const auto &param : method.params) {
                    info.params.push_back(ResolveType(param.first));
                }
                info.wasm_name = MangleFunctionName(method, def.name);
                functions_[def.name + "." + method.name] = info;
                structs_[def.name].methods[method.name] = info.decl;
            }
        }
    }

    std::string MangleFunctionName(const Function &fn, const std::string &owner) {
        if (!owner.empty()) {
            return "$" + owner + "_" + fn.name;
        }
        return "$" + fn.name;
    }

    void BuildStringLiterals() {
        AddStringLiteral("\n");
        AddStringLiteral(".");
        AddStringLiteral("-");
        AddStringLiteral("+");
        AddStringLiteral("e");
        AddStringLiteral("0");
        AddStringLiteral("true");
        AddStringLiteral("false");
        for (const auto &fn : program_.functions) {
            for (const auto &stmt : fn.body) {
                CollectStrings(stmt);
            }
        }
        for (const auto &def : program_.structs) {
            for (const auto &method : def.methods) {
                for (const auto &stmt : method.body) {
                    CollectStrings(stmt);
                }
            }
        }
        heap_start_ = Align8(data_cursor_);
    }

    void CollectStrings(const StmtPtr &stmt) {
        if (!stmt) {
            return;
        }
        switch (stmt->kind) {
            case StmtKind::VarDecl:
            case StmtKind::Assign:
            case StmtKind::ExprStmt:
            case StmtKind::Return:
                CollectStrings(stmt->expr);
                CollectStrings(stmt->target);
                break;
            case StmtKind::If:
                CollectStrings(stmt->expr);
                for (const auto &s : stmt->then_body) {
                    CollectStrings(s);
                }
                for (const auto &s : stmt->else_body) {
                    CollectStrings(s);
                }
                break;
            case StmtKind::While:
                CollectStrings(stmt->expr);
                for (const auto &s : stmt->body) {
                    CollectStrings(s);
                }
                break;
        }
    }

    void CollectStrings(const ExprPtr &expr) {
        if (!expr) {
            return;
        }
        if (expr->kind == ExprKind::Call && expr->base && expr->base->kind == ExprKind::Var &&
            expr->base->text == "print" && !expr->args.empty() &&
            expr->args[0]->kind == ExprKind::StringLit) {
            if (expr->args.size() > 1 || NeedsFormat(expr->args[0]->text)) {
                AddFormatLiterals(expr->args[0]->text);
            }
        }
        if (expr->kind == ExprKind::StringLit) {
            AddStringLiteral(expr->text);
        }
        if (expr->left) {
            CollectStrings(expr->left);
        }
        if (expr->right) {
            CollectStrings(expr->right);
        }
        if (expr->base) {
            CollectStrings(expr->base);
        }
        if (expr->new_size) {
            CollectStrings(expr->new_size);
        }
        for (const auto &arg : expr->args) {
            CollectStrings(arg);
        }
    }

    int64_t AddStringLiteral(const std::string &value) {
        auto it = string_offsets_.find(value);
        if (it != string_offsets_.end()) {
            return it->second;
        }
        int64_t offset = Align8(data_cursor_);
        int64_t length = static_cast<int64_t>(value.size());
        std::string bytes;
        bytes.resize(8 + value.size());
        for (int i = 0; i < 8; ++i) {
            bytes[i] = static_cast<char>((length >> (i * 8)) & 0xFF);
        }
        std::copy(value.begin(), value.end(), bytes.begin() + 8);
        data_segments_.push_back({offset, bytes});
        string_offsets_[value] = offset;
        data_cursor_ = offset + static_cast<int64_t>(bytes.size());
        return offset;
    }

    void AddFormatLiterals(const std::string &format) {
        std::string literal;
        for (size_t i = 0; i < format.size(); ++i) {
            char c = format[i];
            if (c == '%' && i + 1 < format.size()) {
                char next = format[i + 1];
                if (next == '%') {
                    literal.push_back('%');
                    i++;
                    continue;
                }
                if (!literal.empty()) {
                    AddStringLiteral(literal);
                    literal.clear();
                }
                if (next == 'i' || next == 'b' || next == 's') {
                    i++;
                    continue;
                }
                if (next == 'r' || next == 'e') {
                    i++;
                    if (i + 1 < format.size() && format[i + 1] == '{') {
                        i += 2;
                        if (i >= format.size() || !std::isdigit(static_cast<unsigned char>(format[i]))) {
                            throw CompileError("Format precision requires digits");
                        }
                        while (i < format.size() && std::isdigit(static_cast<unsigned char>(format[i]))) {
                            i++;
                        }
                        if (i >= format.size() || format[i] != '}') {
                            throw CompileError("Format precision missing '}'");
                        }
                    }
                    continue;
                }
                throw CompileError("Unsupported format specifier in print");
            }
            literal.push_back(c);
        }
        if (!literal.empty()) {
            AddStringLiteral(literal);
        }
    }

    static int64_t Align8(int64_t value) {
        return (value + 7) & ~static_cast<int64_t>(7);
    }

    void EmitModule() {
        out_ << "(module\n";
        out_ << "  (import \"wasi_snapshot_preview1\" \"fd_write\" (func $fd_write (param i32 i32 i32 i32) (result i32)))\n";
        out_ << "  (import \"wasi_snapshot_preview1\" \"args_sizes_get\" (func $args_sizes_get (param i32 i32) (result i32)))\n";
        out_ << "  (import \"wasi_snapshot_preview1\" \"args_get\" (func $args_get (param i32 i32) (result i32)))\n";
        out_ << "  (memory (export \"memory\") 1)\n";
        out_ << "  (global $heap (mut i64) (i64.const " << heap_start_ << "))\n";
        EmitDataSegments();
        RuntimeEmitter runtime(out_, string_offsets_);
        runtime.Emit();
        for (const auto &fn : program_.functions) {
            EmitFunction(fn, "");
        }
        for (const auto &def : program_.structs) {
            EmitStructInit(def);
            for (const auto &method : def.methods) {
                EmitFunction(method, def.name);
            }
        }
        EmitStart();
        out_ << ")\n";
    }

    void EmitDataSegments() {
        for (const auto &seg : data_segments_) {
            out_ << "  (data (i32.const " << seg.first << ") \"" << EscapeBytes(seg.second) << "\")\n";
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


    void EmitFunction(const Function &fn, const std::string &owner) {
        FunctionInfo &info = functions_.at(owner.empty() ? fn.name : owner + "." + fn.name);
        out_ << "  (func " << info.wasm_name;
        if (owner.empty()) {
            out_ << " (export \"" << fn.name << "\")";
        }
        Env env;
        env.current_struct = owner;
        for (size_t i = 0; i < info.params.size(); ++i) {
            std::string param_name;
            std::string wasm_type = WasmType(info.params[i]);
            if (owner.empty() || i > 0) {
                param_name = "$p" + std::to_string(i);
            } else {
                param_name = "$this";
            }
            out_ << " (param " << param_name << " " << wasm_type << ")";
            LocalInfo local{param_name, info.params[i]};
            if (owner.empty() || i > 0) {
                env.params[fn.params[i - (owner.empty() ? 0 : 1)].second] = local;
            } else {
                env.params["this"] = local;
            }
        }
        if (info.return_type->kind != Type::Kind::Void) {
            out_ << " (result " << WasmType(info.return_type) << ")";
        }
        std::vector<LocalInfo> locals;
        for (const auto &stmt : fn.body) {
            CollectLocals(stmt, env, locals);
        }
        out_ << " (local $tmp0 i64) (local $tmp1 i64) (local $tmp2 i64) (local $tmpf f64)";
        for (const auto &local : locals) {
            out_ << " (local " << local.wasm_name << " " << WasmType(local.type) << ")";
        }
        out_ << "\n";
        for (const auto &stmt : fn.body) {
            EmitStmt(stmt, env);
        }
        if (info.return_type->kind == Type::Kind::Void) {
            out_ << "    (nop)\n";
        } else {
            EmitZero(info.return_type);
            out_ << "    return\n";
        }
        out_ << "  )\n";
    }

    void CollectLocals(const StmtPtr &stmt, Env &env, std::vector<LocalInfo> &locals) {
        if (!stmt) {
            return;
        }
        if (stmt->kind == StmtKind::VarDecl) {
            LocalInfo local;
            local.type = ResolveType(stmt->var_type);
            local.wasm_name = "$v" + stmt->var_name;
            env.locals[stmt->var_name] = local;
            locals.push_back(local);
        }
        if (stmt->kind == StmtKind::If) {
            for (const auto &s : stmt->then_body) {
                CollectLocals(s, env, locals);
            }
            for (const auto &s : stmt->else_body) {
                CollectLocals(s, env, locals);
            }
        }
        if (stmt->kind == StmtKind::While) {
            for (const auto &s : stmt->body) {
                CollectLocals(s, env, locals);
            }
        }
    }

    void EmitStmt(const StmtPtr &stmt, Env &env) {
        switch (stmt->kind) {
            case StmtKind::VarDecl: {
                auto it = env.locals.find(stmt->var_name);
                if (it == env.locals.end()) {
                    throw CompileError("Unknown local variable " + stmt->var_name);
                }
                if (stmt->expr) {
                    auto expr_type = EmitExpr(stmt->expr, env);
                    RequireSameType(it->second.type, expr_type, stmt->line);
                    out_ << "    local.set " << it->second.wasm_name << "\n";
                } else {
                    EmitZero(it->second.type);
                    out_ << "    local.set " << it->second.wasm_name << "\n";
                }
                break;
            }
            case StmtKind::Assign: {
                EmitAssignment(stmt->target, stmt->expr, env);
                break;
            }
            case StmtKind::ExprStmt: {
                auto expr_type = EmitExpr(stmt->expr, env);
                if (expr_type->kind != Type::Kind::Void) {
                    out_ << "    drop\n";
                }
                break;
            }
            case StmtKind::Return: {
                if (stmt->expr) {
                    EmitExpr(stmt->expr, env);
                    out_ << "    return\n";
                } else {
                    out_ << "    return\n";
                }
                break;
            }
            case StmtKind::If: {
                EmitCondition(stmt->expr, env);
                out_ << "    if\n";
                for (const auto &s : stmt->then_body) {
                    EmitStmt(s, env);
                }
                if (!stmt->else_body.empty()) {
                    out_ << "    else\n";
                    for (const auto &s : stmt->else_body) {
                        EmitStmt(s, env);
                    }
                }
                out_ << "    end\n";
                break;
            }
            case StmtKind::While: {
                out_ << "    block\n";
                out_ << "      loop\n";
                EmitCondition(stmt->expr, env);
                out_ << "      i32.eqz\n";
                out_ << "      br_if 1\n";
                for (const auto &s : stmt->body) {
                    EmitStmt(s, env);
                }
                out_ << "      br 0\n";
                out_ << "      end\n";
                out_ << "    end\n";
                break;
            }
        }
    }

    void EmitCondition(const ExprPtr &expr, Env &env) {
        auto type = EmitExpr(expr, env);
        if (type->kind != Type::Kind::Bool) {
            throw CompileError("Condition must be bool at line " + std::to_string(expr->line));
        }
        out_ << "    i32.wrap_i64\n";
    }

    void EmitAssignment(const ExprPtr &target, const ExprPtr &value, Env &env) {
        if (target->kind == ExprKind::Var) {
            auto it = env.locals.find(target->text);
            if (it == env.locals.end()) {
                auto pit = env.params.find(target->text);
                if (pit == env.params.end()) {
                    if (!env.current_struct.empty()) {
                        const auto &info = structs_.at(env.current_struct);
                        auto fit = info.field_map.find(target->text);
                        if (fit != info.field_map.end()) {
                            out_ << "    local.get $this\n";
                            out_ << "    i64.const " << fit->second.offset << "\n";
                            out_ << "    i64.add\n";
                            out_ << "    local.set $tmp2\n";
                            auto rhs_type = EmitExpr(value, env);
                            RequireSameType(fit->second.type, rhs_type, value->line);
                            if (rhs_type->kind == Type::Kind::Real) {
                                out_ << "    local.set $tmpf\n";
                                out_ << "    local.get $tmp2\n";
                                out_ << "    i32.wrap_i64\n";
                                out_ << "    local.get $tmpf\n";
                                out_ << "    f64.store\n";
                            } else {
                                out_ << "    local.set $tmp1\n";
                                out_ << "    local.get $tmp2\n";
                                out_ << "    i32.wrap_i64\n";
                                out_ << "    local.get $tmp1\n";
                                out_ << "    i64.store\n";
                            }
                            return;
                        }
                    }
                    throw CompileError("Unknown variable " + target->text + " at line " + std::to_string(target->line));
                }
                auto rhs_type = EmitExpr(value, env);
                RequireSameType(pit->second.type, rhs_type, value->line);
                out_ << "    local.set " << pit->second.wasm_name << "\n";
                return;
            }
            auto rhs_type = EmitExpr(value, env);
            RequireSameType(it->second.type, rhs_type, value->line);
            out_ << "    local.set " << it->second.wasm_name << "\n";
            return;
        }
        auto addr_type = EmitAddress(target, env);
        out_ << "    local.set $tmp2\n";
        auto rhs_type = EmitExpr(value, env);
        RequireSameType(addr_type, rhs_type, value->line);
        if (rhs_type->kind == Type::Kind::Real) {
            out_ << "    local.set $tmpf\n";
            out_ << "    local.get $tmp2\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmpf\n";
            out_ << "    f64.store\n";
        } else {
            out_ << "    local.set $tmp1\n";
            out_ << "    local.get $tmp2\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmp1\n";
            out_ << "    i64.store\n";
        }
    }

    std::shared_ptr<Type> EmitAddress(const ExprPtr &expr, Env &env) {
        if (expr->kind == ExprKind::Field) {
            auto base_type = EmitExpr(expr->base, env);
            if (base_type->kind != Type::Kind::Struct) {
                throw CompileError("Field access on non-struct at line " + std::to_string(expr->line));
            }
            const auto &info = structs_.at(base_type->name);
            auto it = info.field_map.find(expr->field);
            if (it == info.field_map.end()) {
                throw CompileError("Unknown field " + expr->field + " on struct " + base_type->name);
            }
            out_ << "    i64.const " << it->second.offset << "\n";
            out_ << "    i64.add\n";
            return it->second.type;
        }
        if (expr->kind == ExprKind::Index) {
            auto base_type = EmitExpr(expr->base, env);
            if (base_type->kind != Type::Kind::Array) {
                throw CompileError("Indexing non-array at line " + std::to_string(expr->line));
            }
            out_ << "    local.set $tmp0\n";
            auto index_type = EmitExpr(expr->left, env);
            if (index_type->kind != Type::Kind::Int) {
                throw CompileError("Array index must be int at line " + std::to_string(expr->line));
            }
            out_ << "    local.set $tmp1\n";
            int64_t elem_size = TypeSize(base_type->element);
            out_ << "    local.get $tmp0\n";
            out_ << "    i64.const 8\n";
            out_ << "    i64.add\n";
            out_ << "    local.get $tmp1\n";
            out_ << "    i64.const " << elem_size << "\n";
            out_ << "    i64.mul\n";
            out_ << "    i64.add\n";
            return base_type->element;
        }
        throw CompileError("Invalid assignment target at line " + std::to_string(expr->line));
    }

    std::shared_ptr<Type> EmitExpr(const ExprPtr &expr, Env &env) {
        switch (expr->kind) {
            case ExprKind::IntLit:
                out_ << "    i64.const " << expr->int_value << "\n";
                return ResolveType(TypeSpec{"int", 0, false});
            case ExprKind::RealLit:
                out_ << "    f64.const " << expr->real_value << "\n";
                return ResolveType(TypeSpec{"real", 0, false});
            case ExprKind::StringLit: {
                int64_t offset = string_offsets_.at(expr->text);
                out_ << "    i64.const " << offset << "\n";
                return ResolveType(TypeSpec{"string", 0, false});
            }
            case ExprKind::BoolLit:
                out_ << "    i64.const " << (expr->bool_value ? 1 : 0) << "\n";
                return ResolveType(TypeSpec{"bool", 0, false});
            case ExprKind::Var: {
                auto it = env.locals.find(expr->text);
                if (it != env.locals.end()) {
                    out_ << "    local.get " << it->second.wasm_name << "\n";
                    return it->second.type;
                }
                auto pit = env.params.find(expr->text);
                if (pit != env.params.end()) {
                    out_ << "    local.get " << pit->second.wasm_name << "\n";
                    return pit->second.type;
                }
                if (!env.current_struct.empty()) {
                    const auto &info = structs_.at(env.current_struct);
                    auto fit = info.field_map.find(expr->text);
                    if (fit != info.field_map.end()) {
                        out_ << "    local.get $this\n";
                        out_ << "    i64.const " << fit->second.offset << "\n";
                        out_ << "    i64.add\n";
                        EmitLoad(fit->second.type);
                        return fit->second.type;
                    }
                }
                throw CompileError("Unknown identifier " + expr->text + " at line " + std::to_string(expr->line));
            }
            case ExprKind::Unary: {
                auto operand = EmitExpr(expr->left, env);
                if (expr->op == "-") {
                    if (operand->kind == Type::Kind::Int) {
                        out_ << "    i64.const -1\n";
                        out_ << "    i64.mul\n";
                        return operand;
                    }
                    if (operand->kind == Type::Kind::Real) {
                        out_ << "    f64.neg\n";
                        return operand;
                    }
                    throw CompileError("Unary '-' requires int or real at line " + std::to_string(expr->line));
                }
                if (expr->op == "!") {
                    if (operand->kind != Type::Kind::Bool) {
                        throw CompileError("Unary '!' requires bool at line " + std::to_string(expr->line));
                    }
                    out_ << "    i64.eqz\n";
                    out_ << "    i64.extend_i32_u\n";
                    return operand;
                }
                throw CompileError("Unknown unary operator at line " + std::to_string(expr->line));
            }
            case ExprKind::Binary:
                return EmitBinary(expr, env);
            case ExprKind::Field:
                return EmitField(expr, env);
            case ExprKind::Index:
                return EmitIndex(expr, env);
            case ExprKind::Call:
                return EmitCall(expr, env);
            case ExprKind::NewExpr:
                return EmitNew(expr, env);
        }
        throw CompileError("Unhandled expression at line " + std::to_string(expr->line));
    }

    std::shared_ptr<Type> InferExprType(const ExprPtr &expr, Env &env) {
        switch (expr->kind) {
            case ExprKind::IntLit:
                return ResolveType(TypeSpec{"int", 0, false});
            case ExprKind::RealLit:
                return ResolveType(TypeSpec{"real", 0, false});
            case ExprKind::StringLit:
                return ResolveType(TypeSpec{"string", 0, false});
            case ExprKind::BoolLit:
                return ResolveType(TypeSpec{"bool", 0, false});
            case ExprKind::Var: {
                auto it = env.locals.find(expr->text);
                if (it != env.locals.end()) {
                    return it->second.type;
                }
                auto pit = env.params.find(expr->text);
                if (pit != env.params.end()) {
                    return pit->second.type;
                }
                if (!env.current_struct.empty()) {
                    const auto &info = structs_.at(env.current_struct);
                    auto fit = info.field_map.find(expr->text);
                    if (fit != info.field_map.end()) {
                        return fit->second.type;
                    }
                }
                throw CompileError("Unknown identifier " + expr->text + " at line " + std::to_string(expr->line));
            }
            case ExprKind::Unary: {
                auto operand = InferExprType(expr->left, env);
                if (expr->op == "-") {
                    if (operand->kind == Type::Kind::Int || operand->kind == Type::Kind::Real) {
                        return operand;
                    }
                }
                if (expr->op == "!") {
                    if (operand->kind == Type::Kind::Bool) {
                        return operand;
                    }
                }
                throw CompileError("Invalid unary operator at line " + std::to_string(expr->line));
            }
            case ExprKind::Binary: {
                auto left = InferExprType(expr->left, env);
                auto right = InferExprType(expr->right, env);
                if (expr->op == "+" || expr->op == "-" || expr->op == "*" || expr->op == "/" || expr->op == "%") {
                    if (left->kind == Type::Kind::Int && right->kind == Type::Kind::Int) {
                        return left;
                    }
                    if (left->kind == Type::Kind::Real && right->kind == Type::Kind::Real) {
                        return left;
                    }
                    if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
                        (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
                        return ResolveType(TypeSpec{"real", 0, false});
                    }
                    throw CompileError("Arithmetic requires int or real at line " + std::to_string(expr->line));
                }
                if (expr->op == "==" || expr->op == "!=" || expr->op == "<" || expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
                    if (left->kind == right->kind) {
                        if (left->kind == Type::Kind::Int || left->kind == Type::Kind::Bool || left->kind == Type::Kind::Real) {
                            return ResolveType(TypeSpec{"bool", 0, false});
                        }
                    }
                    if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
                        (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
                        return ResolveType(TypeSpec{"bool", 0, false});
                    }
                    throw CompileError("Comparison not supported for this type at line " + std::to_string(expr->line));
                }
                if (expr->op == "and" || expr->op == "or") {
                    if (left->kind == Type::Kind::Bool && right->kind == Type::Kind::Bool) {
                        return left;
                    }
                    throw CompileError("Logical operators require bool at line " + std::to_string(expr->line));
                }
                throw CompileError("Unknown binary operator at line " + std::to_string(expr->line));
            }
            case ExprKind::Field: {
                auto base_type = InferExprType(expr->base, env);
                if (base_type->kind != Type::Kind::Struct) {
                    throw CompileError("Field access on non-struct at line " + std::to_string(expr->line));
                }
                const auto &info = structs_.at(base_type->name);
                auto it = info.field_map.find(expr->field);
                if (it == info.field_map.end()) {
                    throw CompileError("Unknown field " + expr->field + " on struct " + base_type->name);
                }
                return it->second.type;
            }
            case ExprKind::Index: {
                auto base_type = InferExprType(expr->base, env);
                if (base_type->kind != Type::Kind::Array) {
                    throw CompileError("Indexing non-array at line " + std::to_string(expr->line));
                }
                return base_type->element;
            }
            case ExprKind::Call: {
                if (expr->base->kind == ExprKind::Var) {
                    std::string name = expr->base->text;
                    if (name == "print") {
                        return ResolveType(TypeSpec{"void", 0, true});
                    }
                    if (name == "sqrt") {
                        return ResolveType(TypeSpec{"real", 0, false});
                    }
                    auto it = functions_.find(name);
                    if (it == functions_.end()) {
                        throw CompileError("Unknown function " + name + " at line " + std::to_string(expr->line));
                    }
                    return it->second.return_type;
                }
                if (expr->base->kind == ExprKind::Field) {
                    auto field = expr->base;
                    auto base_type = InferExprType(field->base, env);
                    if ((base_type->kind == Type::Kind::Array || base_type->kind == Type::Kind::String) && field->field == "length") {
                        return ResolveType(TypeSpec{"int", 0, false});
                    }
                    if (base_type->kind != Type::Kind::Struct) {
                        throw CompileError("Method call on non-struct at line " + std::to_string(expr->line));
                    }
                    std::string method_name = base_type->name + "." + field->field;
                    auto it = functions_.find(method_name);
                    if (it == functions_.end()) {
                        throw CompileError("Unknown method " + field->field + " on struct " + base_type->name);
                    }
                    return it->second.return_type;
                }
                throw CompileError("Unsupported call expression at line " + std::to_string(expr->line));
            }
            case ExprKind::NewExpr:
                if (expr->new_size) {
                    auto base = ResolveType(expr->new_type);
                    if (base->kind == Type::Kind::Array) {
                        return base;
                    }
                    return std::make_shared<Type>(Type{Type::Kind::Array, "", base});
                }
                return ResolveType(expr->new_type);
        }
        throw CompileError("Unhandled expression at line " + std::to_string(expr->line));
    }

    void EmitLoad(const std::shared_ptr<Type> &type) {
        out_ << "    i32.wrap_i64\n";
        if (type->kind == Type::Kind::Real) {
            out_ << "    f64.load\n";
        } else {
            out_ << "    i64.load\n";
        }
    }

    std::shared_ptr<Type> EmitBinary(const ExprPtr &expr, Env &env) {
        auto left = InferExprType(expr->left, env);
        auto right = InferExprType(expr->right, env);
        if (expr->op == "+" || expr->op == "-" || expr->op == "*" || expr->op == "/" || expr->op == "%") {
            if (left->kind == Type::Kind::Int && right->kind == Type::Kind::Int) {
                EmitExpr(expr->left, env);
                EmitExpr(expr->right, env);
                if (expr->op == "+") out_ << "    i64.add\n";
                if (expr->op == "-") out_ << "    i64.sub\n";
                if (expr->op == "*") out_ << "    i64.mul\n";
                if (expr->op == "/") out_ << "    i64.div_s\n";
                if (expr->op == "%") out_ << "    i64.rem_s\n";
                return left;
            }
            if (left->kind == Type::Kind::Real && right->kind == Type::Kind::Real) {
                EmitExpr(expr->left, env);
                EmitExpr(expr->right, env);
                if (expr->op == "+") out_ << "    f64.add\n";
                if (expr->op == "-") out_ << "    f64.sub\n";
                if (expr->op == "*") out_ << "    f64.mul\n";
                if (expr->op == "/") out_ << "    f64.div\n";
                return left;
            }
            if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
                (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
                if (left->kind == Type::Kind::Int) {
                    EmitExpr(expr->left, env);
                    out_ << "    f64.convert_i64_s\n";
                    EmitExpr(expr->right, env);
                } else {
                    EmitExpr(expr->left, env);
                    EmitExpr(expr->right, env);
                    out_ << "    f64.convert_i64_s\n";
                }
                if (expr->op == "+") out_ << "    f64.add\n";
                if (expr->op == "-") out_ << "    f64.sub\n";
                if (expr->op == "*") out_ << "    f64.mul\n";
                if (expr->op == "/") out_ << "    f64.div\n";
                return ResolveType(TypeSpec{"real", 0, false});
            }
            throw CompileError("Arithmetic requires int or real at line " + std::to_string(expr->line));
        }
        if (expr->op == "==" || expr->op == "!=" || expr->op == "<" || expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
            if ((left->kind == Type::Kind::Int || left->kind == Type::Kind::Bool) && right->kind == left->kind) {
                EmitExpr(expr->left, env);
                EmitExpr(expr->right, env);
                EmitIntCompare(expr->op);
                return ResolveType(TypeSpec{"bool", 0, false});
            }
            if (left->kind == Type::Kind::Real && right->kind == Type::Kind::Real) {
                EmitExpr(expr->left, env);
                EmitExpr(expr->right, env);
                EmitFloatCompare(expr->op);
                return ResolveType(TypeSpec{"bool", 0, false});
            }
            if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
                (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
                if (left->kind == Type::Kind::Int) {
                    EmitExpr(expr->left, env);
                    out_ << "    f64.convert_i64_s\n";
                    EmitExpr(expr->right, env);
                } else {
                    EmitExpr(expr->left, env);
                    EmitExpr(expr->right, env);
                    out_ << "    f64.convert_i64_s\n";
                }
                EmitFloatCompare(expr->op);
                return ResolveType(TypeSpec{"bool", 0, false});
            }
            throw CompileError("Comparison not supported for this type at line " + std::to_string(expr->line));
        }
        if (expr->op == "and" || expr->op == "or") {
            if (left->kind != Type::Kind::Bool || right->kind != Type::Kind::Bool) {
                throw CompileError("Logical operators require bool at line " + std::to_string(expr->line));
            }
            EmitExpr(expr->left, env);
            EmitExpr(expr->right, env);
            if (expr->op == "and") {
                out_ << "    i64.and\n";
            } else {
                out_ << "    i64.or\n";
            }
            return left;
        }
        throw CompileError("Unknown binary operator at line " + std::to_string(expr->line));
    }

    void EmitIntCompare(const std::string &op) {
        if (op == "==") out_ << "    i64.eq\n";
        if (op == "!=") out_ << "    i64.ne\n";
        if (op == "<") out_ << "    i64.lt_s\n";
        if (op == "<=") out_ << "    i64.le_s\n";
        if (op == ">") out_ << "    i64.gt_s\n";
        if (op == ">=") out_ << "    i64.ge_s\n";
        out_ << "    i64.extend_i32_u\n";
    }

    void EmitFloatCompare(const std::string &op) {
        if (op == "==") out_ << "    f64.eq\n";
        if (op == "!=") out_ << "    f64.ne\n";
        if (op == "<") out_ << "    f64.lt\n";
        if (op == "<=") out_ << "    f64.le\n";
        if (op == ">") out_ << "    f64.gt\n";
        if (op == ">=") out_ << "    f64.ge\n";
        out_ << "    i64.extend_i32_u\n";
    }

    std::shared_ptr<Type> EmitField(const ExprPtr &expr, Env &env) {
        auto base_type = EmitExpr(expr->base, env);
        if (base_type->kind != Type::Kind::Struct) {
            throw CompileError("Field access on non-struct at line " + std::to_string(expr->line));
        }
        const auto &info = structs_.at(base_type->name);
        auto it = info.field_map.find(expr->field);
        if (it == info.field_map.end()) {
            throw CompileError("Unknown field " + expr->field + " on struct " + base_type->name);
        }
        out_ << "    i64.const " << it->second.offset << "\n";
        out_ << "    i64.add\n";
        EmitLoad(it->second.type);
        return it->second.type;
    }

    std::shared_ptr<Type> EmitIndex(const ExprPtr &expr, Env &env) {
        auto base_type = EmitExpr(expr->base, env);
        if (base_type->kind != Type::Kind::Array) {
            throw CompileError("Indexing non-array at line " + std::to_string(expr->line));
        }
        out_ << "    local.set $tmp0\n";
        auto index_type = EmitExpr(expr->left, env);
        if (index_type->kind != Type::Kind::Int) {
            throw CompileError("Array index must be int at line " + std::to_string(expr->line));
        }
        out_ << "    local.set $tmp1\n";
        int64_t elem_size = TypeSize(base_type->element);
        out_ << "    local.get $tmp0\n";
        out_ << "    i64.const 8\n";
        out_ << "    i64.add\n";
        out_ << "    local.get $tmp1\n";
        out_ << "    i64.const " << elem_size << "\n";
        out_ << "    i64.mul\n";
        out_ << "    i64.add\n";
        EmitLoad(base_type->element);
        return base_type->element;
    }

    std::shared_ptr<Type> EmitCall(const ExprPtr &expr, Env &env) {
        if (expr->base->kind == ExprKind::Var) {
            std::string name = expr->base->text;
            if (name == "print") {
                if (expr->args.empty()) {
                    throw CompileError("print expects at least 1 argument at line " + std::to_string(expr->line));
                }
                if (expr->args[0]->kind == ExprKind::StringLit) {
                    bool uses_format = expr->args.size() > 1 || NeedsFormat(expr->args[0]->text);
                    if (uses_format) {
                        EmitFormattedPrint(expr->args[0]->text, expr->args, env, expr->line);
                        return ResolveType(TypeSpec{"void", 0, true});
                    }
                }
                if (expr->args.size() > 1 && expr->args[0]->kind != ExprKind::StringLit) {
                    EmitRuntimeFormatCall(expr->args, env, expr->line);
                    return ResolveType(TypeSpec{"void", 0, true});
                }
                if (expr->args.size() != 1) {
                    throw CompileError("print expects 1 argument or a format string at line " + std::to_string(expr->line));
                }
                auto arg_type = EmitExpr(expr->args[0], env);
                if (arg_type->kind == Type::Kind::Int) {
                    out_ << "    call $print_i64\n";
                    return ResolveType(TypeSpec{"void", 0, true});
                }
                if (arg_type->kind == Type::Kind::Bool) {
                    out_ << "    call $print_bool\n";
                    return ResolveType(TypeSpec{"void", 0, true});
                }
                if (arg_type->kind == Type::Kind::String) {
                    out_ << "    call $print_string\n";
                    return ResolveType(TypeSpec{"void", 0, true});
                }
                if (arg_type->kind == Type::Kind::Real) {
                    out_ << "    call $print_f64\n";
                    return ResolveType(TypeSpec{"void", 0, true});
                }
                throw CompileError("print only supports int, bool, real, string at line " + std::to_string(expr->line));
            }
            if (name == "sqrt") {
                if (expr->args.size() != 1) {
                    throw CompileError("sqrt expects 1 argument at line " + std::to_string(expr->line));
                }
                auto arg_type = EmitExpr(expr->args[0], env);
                if (arg_type->kind != Type::Kind::Real) {
                    throw CompileError("sqrt expects real at line " + std::to_string(expr->line));
                }
                out_ << "    f64.sqrt\n";
                return arg_type;
            }
            auto it = functions_.find(name);
            if (it == functions_.end()) {
                throw CompileError("Unknown function " + name + " at line " + std::to_string(expr->line));
            }
            if (expr->args.size() != it->second.params.size()) {
                throw CompileError("Argument count mismatch for " + name);
            }
            for (size_t i = 0; i < expr->args.size(); ++i) {
                auto arg_type = EmitExpr(expr->args[i], env);
                RequireSameType(it->second.params[i], arg_type, expr->line);
            }
            out_ << "    call " << it->second.wasm_name << "\n";
            return it->second.return_type;
        }
        if (expr->base->kind == ExprKind::Field) {
            auto field = expr->base;
            auto base_type = EmitExpr(field->base, env);
            if (base_type->kind == Type::Kind::Array && field->field == "length") {
                if (!expr->args.empty()) {
                    throw CompileError("length() takes no args at line " + std::to_string(expr->line));
                }
                out_ << "    i32.wrap_i64\n";
                out_ << "    i64.load\n";
                return ResolveType(TypeSpec{"int", 0, false});
            }
            if (base_type->kind == Type::Kind::String && field->field == "length") {
                if (!expr->args.empty()) {
                    throw CompileError("length() takes no args at line " + std::to_string(expr->line));
                }
                out_ << "    i32.wrap_i64\n";
                out_ << "    i64.load\n";
                return ResolveType(TypeSpec{"int", 0, false});
            }
            if (base_type->kind != Type::Kind::Struct) {
                throw CompileError("Method call on non-struct at line " + std::to_string(expr->line));
            }
            std::string method_name = base_type->name + "." + field->field;
            auto it = functions_.find(method_name);
            if (it == functions_.end()) {
                throw CompileError("Unknown method " + field->field + " on struct " + base_type->name);
            }
            if (expr->args.size() + 1 != it->second.params.size()) {
                throw CompileError("Argument count mismatch for method " + field->field);
            }
            for (size_t i = 0; i < expr->args.size(); ++i) {
                auto arg_type = EmitExpr(expr->args[i], env);
                RequireSameType(it->second.params[i + 1], arg_type, expr->line);
            }
            out_ << "    call " << it->second.wasm_name << "\n";
            return it->second.return_type;
        }
        throw CompileError("Unsupported call expression at line " + std::to_string(expr->line));
    }

    bool NeedsFormat(const std::string &format) const {
        for (char c : format) {
            if (c == '%' || c == '\n') {
                return true;
            }
        }
        return false;
    }

    void EmitFormattedPrint(const std::string &format, const std::vector<ExprPtr> &args, Env &env, int line) {
        size_t arg_index = 1;
        std::string literal;
        auto flush_literal = [&]() {
            if (literal.empty()) {
                return;
            }
            auto it = string_offsets_.find(literal);
            if (it == string_offsets_.end()) {
                throw CompileError("Missing format literal in string table");
            }
            int64_t offset = it->second;
            out_ << "    i32.const " << (offset + 8) << "\n";
            out_ << "    i32.const " << literal.size() << "\n";
            out_ << "    call $write_bytes\n";
            literal.clear();
        };
        for (size_t i = 0; i < format.size(); ++i) {
            char c = format[i];
            if (c == '%' && i + 1 < format.size()) {
                char next = format[i + 1];
                if (next == '%') {
                    literal.push_back('%');
                    i++;
                    continue;
                }
                flush_literal();
                if (arg_index >= args.size()) {
                    throw CompileError("Not enough arguments for format string");
                }
                int precision = 6;
                bool has_precision = false;
                if ((next == 'r' || next == 'e') && i + 2 < format.size() && format[i + 2] == '{') {
                    size_t j = i + 3;
                    if (j >= format.size() || !std::isdigit(static_cast<unsigned char>(format[j]))) {
                        throw CompileError("Format precision requires digits");
                    }
                    precision = 0;
                    while (j < format.size() && std::isdigit(static_cast<unsigned char>(format[j]))) {
                        precision = precision * 10 + (format[j] - '0');
                        j++;
                    }
                    if (j >= format.size() || format[j] != '}') {
                        throw CompileError("Format precision missing '}'");
                    }
                    has_precision = true;
                    i = j;
                } else {
                    i++;
                }

                ExprPtr arg = args[arg_index++];
                auto arg_type = EmitExpr(arg, env);
                if (next == 'i') {
                    if (arg_type->kind != Type::Kind::Int) {
                        throw CompileError("Format %i expects int at line " + std::to_string(line));
                    }
                    out_ << "    call $print_i64_raw\n";
                } else if (next == 'r') {
                    if (arg_type->kind != Type::Kind::Real) {
                        throw CompileError("Format %r expects real at line " + std::to_string(line));
                    }
                    if (has_precision) {
                        out_ << "    i32.const " << precision << "\n";
                        out_ << "    call $print_f64_prec\n";
                    } else {
                        out_ << "    call $print_f64_raw\n";
                    }
                } else if (next == 'e') {
                    if (arg_type->kind != Type::Kind::Real) {
                        throw CompileError("Format %e expects real at line " + std::to_string(line));
                    }
                    out_ << "    i32.const " << precision << "\n";
                    out_ << "    call $print_f64_sci\n";
                } else if (next == 'b') {
                    if (arg_type->kind != Type::Kind::Bool) {
                        throw CompileError("Format %b expects bool at line " + std::to_string(line));
                    }
                    out_ << "    call $print_bool_raw\n";
                } else if (next == 's') {
                    if (arg_type->kind != Type::Kind::String) {
                        throw CompileError("Format %s expects string at line " + std::to_string(line));
                    }
                    out_ << "    call $print_string_raw\n";
                } else {
                    throw CompileError("Unsupported format specifier in print");
                }
                continue;
            }
            literal.push_back(c);
        }
        flush_literal();
        if (arg_index != args.size()) {
            throw CompileError("Too many arguments for format string");
        }
    }

    void EmitRuntimeFormatCall(const std::vector<ExprPtr> &args, Env &env, int line) {
        auto fmt_type = InferExprType(args[0], env);
        if (fmt_type->kind != Type::Kind::String) {
            throw CompileError("print format must be string at line " + std::to_string(line));
        }
        size_t count = args.size() - 1;
        EmitExpr(args[0], env);
        out_ << "    local.set $tmp0\n";
        out_ << "    i64.const " << (count * 16) << "\n";
        out_ << "    call $alloc\n";
        out_ << "    local.set $tmp1\n";
        for (size_t i = 0; i < count; ++i) {
            const auto &arg = args[i + 1];
            auto arg_type = InferExprType(arg, env);
            int tag = 0;
            if (arg_type->kind == Type::Kind::Int) {
                tag = 1;
            } else if (arg_type->kind == Type::Kind::Real) {
                tag = 2;
            } else if (arg_type->kind == Type::Kind::Bool) {
                tag = 3;
            } else if (arg_type->kind == Type::Kind::String) {
                tag = 4;
            } else {
                throw CompileError("Unsupported format argument type at line " + std::to_string(line));
            }
            int64_t offset = static_cast<int64_t>(i) * 16;
            out_ << "    local.get $tmp1\n";
            out_ << "    i64.const " << offset << "\n";
            out_ << "    i64.add\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    i32.const " << tag << "\n";
            out_ << "    i32.store\n";
            if (arg_type->kind == Type::Kind::Real) {
                EmitExpr(arg, env);
                out_ << "    local.set $tmpf\n";
                out_ << "    local.get $tmp1\n";
                out_ << "    i64.const " << (offset + 8) << "\n";
                out_ << "    i64.add\n";
                out_ << "    i32.wrap_i64\n";
                out_ << "    local.get $tmpf\n";
                out_ << "    f64.store\n";
            } else {
                EmitExpr(arg, env);
                out_ << "    local.set $tmp2\n";
                out_ << "    local.get $tmp1\n";
                out_ << "    i64.const " << (offset + 8) << "\n";
                out_ << "    i64.add\n";
                out_ << "    i32.wrap_i64\n";
                out_ << "    local.get $tmp2\n";
                out_ << "    i64.store\n";
            }
        }
        out_ << "    local.get $tmp0\n";
        out_ << "    local.get $tmp1\n";
        out_ << "    i32.const " << count << "\n";
        out_ << "    call $print_format\n";
    }

    std::shared_ptr<Type> EmitNew(const ExprPtr &expr, Env &env) {
        auto type = ResolveType(expr->new_type);
        if (expr->new_size) {
            if (type->kind != Type::Kind::Array) {
                type = std::make_shared<Type>(Type{Type::Kind::Array, "", type});
            }
            auto size_type = EmitExpr(expr->new_size, env);
            if (size_type->kind != Type::Kind::Int) {
                throw CompileError("Array size must be int at line " + std::to_string(expr->line));
            }
            int64_t elem_size = TypeSize(type->element);
            out_ << "    local.set $tmp0\n";
            out_ << "    local.get $tmp0\n";
            out_ << "    i64.const " << elem_size << "\n";
            out_ << "    i64.mul\n";
            out_ << "    i64.const 8\n";
            out_ << "    i64.add\n";
            out_ << "    call $alloc\n";
            out_ << "    local.set $tmp1\n";
            out_ << "    local.get $tmp1\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmp0\n";
            out_ << "    i64.store\n";
            out_ << "    local.get $tmp1\n";
            return type;
        }
        if (type->kind != Type::Kind::Struct) {
            throw CompileError("new without size requires struct type at line " + std::to_string(expr->line));
        }
        int64_t size = structs_.at(type->name).size;
        out_ << "    i64.const " << size << "\n";
        out_ << "    call $alloc\n";
        out_ << "    local.set $tmp0\n";
        out_ << "    local.get $tmp0\n";
        out_ << "    call $init_" << type->name << "\n";
        out_ << "    local.get $tmp0\n";
        return type;
    }

    std::string WasmType(const std::shared_ptr<Type> &type) const {
        if (type->kind == Type::Kind::Real) {
            return "f64";
        }
        if (type->kind == Type::Kind::Void) {
            return "";
        }
        return "i64";
    }

    void EmitZero(const std::shared_ptr<Type> &type) {
        if (type->kind == Type::Kind::Real) {
            out_ << "    f64.const 0\n";
        } else {
            out_ << "    i64.const 0\n";
        }
    }

    void RequireSameType(const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual, int line) {
        if (!IsAssignable(expected, actual)) {
            throw CompileError("Type mismatch at line " + std::to_string(line));
        }
    }

    bool IsAssignable(const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual) {
        if (expected->kind != actual->kind) {
            return false;
        }
        if (expected->kind == Type::Kind::Array) {
            return IsAssignable(expected->element, actual->element);
        }
        if (expected->kind == Type::Kind::Struct) {
            if (expected->name == actual->name) {
                return true;
            }
            std::string current = actual->name;
            while (!current.empty()) {
                auto it = structs_.find(current);
                if (it == structs_.end()) {
                    break;
                }
                if (it->second.parent == expected->name) {
                    return true;
                }
                current = it->second.parent;
            }
            return false;
        }
        return true;
    }

    void EmitStart() {
        auto it = functions_.find("main");
        if (it == functions_.end()) {
            throw CompileError("No main function defined");
        }
        out_ << "  (func $_start (export \"_start\")\n";
        if (it->second.params.empty()) {
            out_ << "    call " << it->second.wasm_name << "\n";
        } else if (it->second.params.size() == 1 &&
                   it->second.params[0]->kind == Type::Kind::Array &&
                   it->second.params[0]->element &&
                   it->second.params[0]->element->kind == Type::Kind::String) {
            out_ << "    call $build_args\n";
            out_ << "    call " << it->second.wasm_name << "\n";
        } else {
            throw CompileError("main must be void main() or void main(string[] args)");
        }
        if (it->second.return_type->kind != Type::Kind::Void) {
            out_ << "    drop\n";
        }
        out_ << "  )\n";
    }

    void EmitStructInit(const StructDef &def) {
        const auto &info = structs_.at(def.name);
        out_ << "  (func $init_" << def.name << " (param $ptr i64) (local $tmp0 i64)\n";
        for (const auto &field : info.fields) {
            if (field.type->kind != Type::Kind::Struct) {
                continue;
            }
            int64_t child_size = structs_.at(field.type->name).size;
            out_ << "    i64.const " << child_size << "\n";
            out_ << "    call $alloc\n";
            out_ << "    local.set $tmp0\n";
            out_ << "    local.get $ptr\n";
            out_ << "    i64.const " << field.offset << "\n";
            out_ << "    i64.add\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmp0\n";
            out_ << "    i64.store\n";
            out_ << "    local.get $tmp0\n";
            out_ << "    call $init_" << field.type->name << "\n";
        }
        out_ << "  )\n";
    }
};

std::string GenerateWasm(const Program &program, const std::unordered_set<std::string> &type_names) {
    CodeGen codegen(program, type_names);
    return codegen.Generate();
}
