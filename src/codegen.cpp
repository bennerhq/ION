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
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "codegen.h"
#include "codegen_types.h"
#include "emitter_expr.h"
#include "emitter_assignment.h"
#include "emitter_expr_access.h"
#include "emitter_expr_var.h"
#include "emitter_expr_new.h"
#include "emitter_condition.h"
#include "expr_type_infer.h"
#include "emitter_format.h"
#include "function_catalog.h"
#include "emitter_function.h"
#include "emitter_module.h"
#include "string_table.h"
#include "emitter_stmt.h"
#include "emitter_struct.h"
#include "struct_layout.h"
#include "type_rules.h"
#include "type_layout.h"
#include "type_resolver.h"


class CodeGen {
public:
    CodeGen(const Program &program, const std::unordered_set<std::string> &type_names);
    std::string Generate();

private:
    struct EmitterRegistry;
    friend struct EmitterRegistry;

    const Program &program_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::ostringstream out_;
    StringLiteralTable string_table_;
    std::unique_ptr<TypeResolver> resolver_;
    std::unique_ptr<EmitterRegistry> emitters_;
    std::unique_ptr<TypeRules> type_rules_;


    std::shared_ptr<Type> ResolveType(const TypeSpec &spec);
    int64_t TypeSize(const std::shared_ptr<Type> &type) const;
    void EmitFunction(const Function &fn, const std::string &owner);
    std::shared_ptr<Type> EmitExpr(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferExprType(const ExprPtr &expr, Env &env);
    void EmitLoad(const std::shared_ptr<Type> &type);
    std::string WasmType(const std::shared_ptr<Type> &type) const;
    void EmitZero(const std::shared_ptr<Type> &type);
    void RequireSameType(const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual, int line);
    void EmitStructInit(const StructDef &def);
};

struct CodeGen::EmitterRegistry {
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using ExprInferrer = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;
    using TypeSizer = std::function<int64_t(const std::shared_ptr<Type> &)>;
    using LoadEmitter = std::function<void(const std::shared_ptr<Type> &)>;
    using WasmTypeEmitter = std::function<std::string(const std::shared_ptr<Type> &)>;
    using ZeroEmitter = std::function<void(const std::shared_ptr<Type> &)>;
    using TypeChecker = std::function<void(const std::shared_ptr<Type> &, const std::shared_ptr<Type> &, int)>;
    using AssignmentFn = std::function<void(const ExprPtr &, const ExprPtr &, Env &)>;
    using ConditionFn = std::function<void(const ExprPtr &, Env &)>;
    using StmtFn = std::function<void(const StmtPtr &, Env &)>;
    using StructInitCaller = std::function<void(const std::string &)>;
    using FunctionFn = std::function<void(const Function &, const std::string &)>;
    using StructInitFn = std::function<void(const StructDef &)>;
    using FunctionInfoGetter = std::function<const FunctionInfo *(const std::string &)>;

    explicit EmitterRegistry(CodeGen &cg)
        : out_(cg.out_),
          string_offsets_(cg.string_table_.Offsets()),
          emit_expr_([&cg](const ExprPtr &expr, Env &env) { return cg.EmitExpr(expr, env); }),
          infer_expr_([&cg](const ExprPtr &expr, Env &env) { return cg.InferExprType(expr, env); }),
          resolve_type_([&cg](const TypeSpec &spec) { return cg.ResolveType(spec); }),
          type_size_([&cg](const std::shared_ptr<Type> &type) { return cg.TypeSize(type); }),
          emit_load_([&cg](const std::shared_ptr<Type> &type) { cg.EmitLoad(type); }),
          wasm_type_([&cg](const std::shared_ptr<Type> &type) { return cg.WasmType(type); }),
          emit_zero_([&cg](const std::shared_ptr<Type> &type) { cg.EmitZero(type); }),
          require_same_type_(
              [&cg](const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual, int line) {
                  cg.RequireSameType(expected, actual, line);
              }),
          emit_assignment_([this](const ExprPtr &target, const ExprPtr &expr, Env &env) {
              MakeAssignmentEmitter().EmitAssignment(target, expr, env);
          }),
          emit_condition_([this](const ExprPtr &expr, Env &env) { MakeConditionEmitter().EmitCondition(expr, env); }),
          emit_stmt_([this](const StmtPtr &stmt, Env &env) { MakeStmtEmitter().EmitStmt(stmt, env); }),
          emit_struct_init_call_([&cg](const std::string &name) { cg.out_ << "    call $init_" << name << "\n"; }),
          emit_function_([&cg](const Function &fn, const std::string &owner) { cg.EmitFunction(fn, owner); }),
          emit_struct_init_([&cg](const StructDef &def) { cg.EmitStructInit(def); }),
          get_function_info_([this](const std::string &name) -> const FunctionInfo * {
              auto it = functions_.find(name);
              return it == functions_.end() ? nullptr : &it->second;
          }),
          functions_(cg.functions_),
          structs_(cg.structs_),
          program_(cg.program_),
          strings_(cg.string_table_) {}

    FormatEmitter &MakeFormatEmitter() {
        if (!format_emitter_) {
            format_emitter_ = std::make_unique<FormatEmitter>(
                out_, string_offsets_, emit_expr_, infer_expr_, resolve_type_);
        }
        return *format_emitter_;
    }

    ExprOperatorEmitter &MakeExprEmitter() {
        if (!expr_emitter_) {
            expr_emitter_ = std::make_unique<ExprOperatorEmitter>(
                out_, emit_expr_, infer_expr_, resolve_type_);
        }
        return *expr_emitter_;
    }

    ExprUnaryEmitter &MakeUnaryEmitter() {
        if (!unary_emitter_) {
            unary_emitter_ = std::make_unique<ExprUnaryEmitter>(out_, emit_expr_);
        }
        return *unary_emitter_;
    }

    ExprTypeInferer &MakeTypeInferer() {
        if (!type_inferer_) {
            type_inferer_ = std::make_unique<ExprTypeInferer>(
                resolve_type_,
                [this](const std::string &name) -> const FunctionInfo * {
                    auto it = functions_.find(name);
                    return it == functions_.end() ? nullptr : &it->second;
                },
                [this](const std::string &name) -> const StructInfo * {
                    auto it = structs_.find(name);
                    return it == structs_.end() ? nullptr : &it->second;
                });
        }
        return *type_inferer_;
    }

    ExprAccessEmitter &MakeAccessEmitter() {
        if (!access_emitter_) {
            access_emitter_ = std::make_unique<ExprAccessEmitter>(
                out_,
                emit_expr_,
                infer_expr_,
                resolve_type_,
                type_size_,
                emit_load_,
                [this](const std::string &name) -> const FunctionInfo * {
                    auto it = functions_.find(name);
                    return it == functions_.end() ? nullptr : &it->second;
                },
                [this](const std::string &name) -> const StructInfo * {
                    auto it = structs_.find(name);
                    return it == structs_.end() ? nullptr : &it->second;
                },
                [this]() -> FormatEmitter & { return MakeFormatEmitter(); },
                require_same_type_);
        }
        return *access_emitter_;
    }

    ExprNewEmitter &MakeNewEmitter() {
        if (!new_emitter_) {
            new_emitter_ = std::make_unique<ExprNewEmitter>(
                out_,
                emit_expr_,
                resolve_type_,
                type_size_,
                [this](const std::string &name) -> const StructInfo * {
                    auto it = structs_.find(name);
                    return it == structs_.end() ? nullptr : &it->second;
                });
        }
        return *new_emitter_;
    }

    ExprVarEmitter &MakeVarEmitter() {
        if (!var_emitter_) {
            var_emitter_ = std::make_unique<ExprVarEmitter>(
                out_,
                emit_load_,
                [this](const std::string &name) -> const StructInfo * {
                    auto it = structs_.find(name);
                    return it == structs_.end() ? nullptr : &it->second;
                });
        }
        return *var_emitter_;
    }

    ::AssignmentEmitter &MakeAssignmentEmitter() {
        if (!assignment_emitter_) {
            assignment_emitter_ = std::make_unique<::AssignmentEmitter>(
                out_,
                emit_expr_,
                type_size_,
                require_same_type_,
                [this](const std::string &name) -> const StructInfo * {
                    auto it = structs_.find(name);
                    return it == structs_.end() ? nullptr : &it->second;
                },
                [this](const ExprPtr &expr, Env &env) -> std::shared_ptr<Type> {
                    return MakeAccessEmitter().EmitIndexAddress(expr, env);
                });
        }
        return *assignment_emitter_;
    }

    ::ConditionEmitter &MakeConditionEmitter() {
        if (!condition_emitter_) {
            condition_emitter_ = std::make_unique<::ConditionEmitter>(out_, emit_expr_);
        }
        return *condition_emitter_;
    }

    ::StmtEmitter &MakeStmtEmitter() {
        if (!stmt_emitter_) {
            stmt_emitter_ = std::make_unique<::StmtEmitter>(
                out_, emit_expr_, emit_assignment_, emit_condition_, emit_zero_, require_same_type_);
        }
        return *stmt_emitter_;
    }

    ::FunctionEmitter &MakeFunctionEmitter() {
        if (!function_emitter_) {
            function_emitter_ = std::make_unique<::FunctionEmitter>(
                out_, wasm_type_, resolve_type_, emit_stmt_, emit_zero_);
        }
        return *function_emitter_;
    }

    ::StructEmitter &MakeStructEmitter() {
        if (!struct_emitter_) {
            struct_emitter_ = std::make_unique<::StructEmitter>(
                out_, structs_, emit_struct_init_call_);
        }
        return *struct_emitter_;
    }

    ::ModuleEmitter &MakeModuleEmitter() {
        if (!module_emitter_) {
            module_emitter_ = std::make_unique<::ModuleEmitter>(
                out_, program_, strings_, emit_function_, emit_struct_init_, get_function_info_);
        }
        return *module_emitter_;
    }

private:
    std::ostream &out_;
    const std::unordered_map<std::string, int64_t> &string_offsets_;
    ExprEmitter emit_expr_;
    ExprInferrer infer_expr_;
    TypeResolver resolve_type_;
    TypeSizer type_size_;
    LoadEmitter emit_load_;
    WasmTypeEmitter wasm_type_;
    ZeroEmitter emit_zero_;
    TypeChecker require_same_type_;
    AssignmentFn emit_assignment_;
    ConditionFn emit_condition_;
    StmtFn emit_stmt_;
    StructInitCaller emit_struct_init_call_;
    FunctionFn emit_function_;
    StructInitFn emit_struct_init_;
    FunctionInfoGetter get_function_info_;
    std::unordered_map<std::string, FunctionInfo> &functions_;
    std::unordered_map<std::string, StructInfo> &structs_;
    const Program &program_;
    const StringLiteralTable &strings_;
    std::unique_ptr<FormatEmitter> format_emitter_;
    std::unique_ptr<ExprOperatorEmitter> expr_emitter_;
    std::unique_ptr<ExprUnaryEmitter> unary_emitter_;
    std::unique_ptr<ExprTypeInferer> type_inferer_;
    std::unique_ptr<ExprAccessEmitter> access_emitter_;
    std::unique_ptr<ExprNewEmitter> new_emitter_;
    std::unique_ptr<ExprVarEmitter> var_emitter_;
    std::unique_ptr<::AssignmentEmitter> assignment_emitter_;
    std::unique_ptr<::ConditionEmitter> condition_emitter_;
    std::unique_ptr<::StmtEmitter> stmt_emitter_;
    std::unique_ptr<::FunctionEmitter> function_emitter_;
    std::unique_ptr<::StructEmitter> struct_emitter_;
    std::unique_ptr<::ModuleEmitter> module_emitter_;
};

CodeGen::CodeGen(const Program &program, const std::unordered_set<std::string> &type_names)
    : program_(program) {
    (void)type_names;
}

std::string CodeGen::Generate() {
    resolver_ = std::make_unique<TypeResolver>(program_, structs_);
    resolver_->InitStructs();
    StructLayout layout(program_, *resolver_, structs_);
    layout.ComputeLayouts();
    FunctionCatalog catalog(
        program_,
        structs_,
        functions_,
        [this](const TypeSpec &spec) { return ResolveType(spec); });
    catalog.Build();
    string_table_.Build(program_);
    type_rules_ = std::make_unique<TypeRules>(structs_);
    emitters_ = std::make_unique<EmitterRegistry>(*this);
    emitters_->MakeModuleEmitter().EmitModule();
    return out_.str();
}

std::shared_ptr<Type> CodeGen::ResolveType(const TypeSpec &spec) {
    return resolver_->Resolve(spec);
}

int64_t CodeGen::TypeSize(const std::shared_ptr<Type> &type) const {
    return TypeLayout::SizeOf(type);
}

void CodeGen::EmitFunction(const Function &fn, const std::string &owner) {
    FunctionInfo &info = functions_.at(owner.empty() ? fn.name : owner + "." + fn.name);
    emitters_->MakeFunctionEmitter().EmitFunction(fn, owner, info);
}

std::shared_ptr<Type> CodeGen::EmitExpr(const ExprPtr &expr, Env &env) {
    switch (expr->kind) {
        case ExprKind::IntLit:
            out_ << "    i64.const " << expr->int_value << "\n";
            return ResolveType(TypeSpec{"int", 0, false});
        case ExprKind::RealLit:
            out_ << "    f64.const " << expr->real_value << "\n";
            return ResolveType(TypeSpec{"real", 0, false});
        case ExprKind::StringLit: {
            int64_t offset = string_table_.Offsets().at(expr->text);
            out_ << "    i64.const " << offset << "\n";
            return ResolveType(TypeSpec{"string", 0, false});
        }
        case ExprKind::BoolLit:
            out_ << "    i64.const " << (expr->bool_value ? 1 : 0) << "\n";
            return ResolveType(TypeSpec{"bool", 0, false});
        case ExprKind::Var:
            return emitters_->MakeVarEmitter().EmitVar(expr, env);
        case ExprKind::Unary:
            return emitters_->MakeUnaryEmitter().EmitUnary(expr, env);
        case ExprKind::Binary:
            return emitters_->MakeExprEmitter().EmitBinary(expr, env);
        case ExprKind::Field:
            return emitters_->MakeAccessEmitter().EmitField(expr, env);
        case ExprKind::Index:
            return emitters_->MakeAccessEmitter().EmitIndex(expr, env);
        case ExprKind::Call:
            return emitters_->MakeAccessEmitter().EmitCall(expr, env);
        case ExprKind::NewExpr:
            return emitters_->MakeNewEmitter().EmitNew(expr, env);
    }
    throw CompileError("Unhandled expression at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> CodeGen::InferExprType(const ExprPtr &expr, Env &env) {
    return emitters_->MakeTypeInferer().Infer(expr, env);
}

void CodeGen::EmitLoad(const std::shared_ptr<Type> &type) {
    out_ << "    i32.wrap_i64\n";
    if (type->kind == Type::Kind::Real) {
        out_ << "    f64.load\n";
    } else {
        out_ << "    i64.load\n";
    }
}

std::string CodeGen::WasmType(const std::shared_ptr<Type> &type) const {
    if (type->kind == Type::Kind::Real) {
        return "f64";
    }
    if (type->kind == Type::Kind::Void) {
        return "";
    }
    return "i64";
}

void CodeGen::EmitZero(const std::shared_ptr<Type> &type) {
    if (type->kind == Type::Kind::Real) {
        out_ << "    f64.const 0\n";
    } else {
        out_ << "    i64.const 0\n";
    }
}

void CodeGen::RequireSameType(const std::shared_ptr<Type> &expected,
                              const std::shared_ptr<Type> &actual,
                              int line) {
    type_rules_->RequireSameType(expected, actual, line);
}

void CodeGen::EmitStructInit(const StructDef &def) {
    emitters_->MakeStructEmitter().EmitStructInit(def);
}

std::string GenerateWasm(const Program &program, const std::unordered_set<std::string> &type_names) {
    CodeGen codegen(program, type_names);
    return codegen.Generate();
}
