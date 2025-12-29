// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "expr_type_infer.h"

#include "common.h"
#include "identifier_lookup.h"

ExprTypeInferer::ExprTypeInferer(TypeResolver resolve_type,
                                 FunctionLookup lookup_function,
                                 StructLookup lookup_struct)
    : resolve_type_(std::move(resolve_type)),
      lookup_function_(std::move(lookup_function)),
      lookup_struct_(std::move(lookup_struct)) {}

std::shared_ptr<Type> ExprTypeInferer::Infer(const ExprPtr &expr, Env &env) {
    switch (expr->kind) {
        case ExprKind::IntLit:
            return resolve_type_(TypeSpec{"int", 0, false});
        case ExprKind::RealLit:
            return resolve_type_(TypeSpec{"real", 0, false});
        case ExprKind::StringLit:
            return resolve_type_(TypeSpec{"string", 0, false});
        case ExprKind::BoolLit:
            return resolve_type_(TypeSpec{"bool", 0, false});
        case ExprKind::Var:
            return InferVar(expr, env);
        case ExprKind::Unary:
            return InferUnary(expr, env);
        case ExprKind::Binary:
            return InferBinary(expr, env);
        case ExprKind::Field:
            return InferField(expr, env);
        case ExprKind::Index:
            return InferIndex(expr, env);
        case ExprKind::Call:
            return InferCall(expr, env);
        case ExprKind::NewExpr:
            return InferNew(expr, env);
    }
    throw CompileError("Unhandled expression at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> ExprTypeInferer::InferVar(const ExprPtr &expr, Env &env) {
    auto result = IdentifierLookup::Find(expr->text, env, lookup_struct_);
    if (result) {
        if (result->kind == IdentifierLookup::Kind::Field) {
            return result->field->type;
        }
        return result->local->type;
    }
    throw CompileError("Unknown identifier " + expr->text + " at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> ExprTypeInferer::InferUnary(const ExprPtr &expr, Env &env) {
    auto operand = Infer(expr->left, env);
    if (expr->op == "-") {
        if (operand->kind == Type::Kind::Int || operand->kind == Type::Kind::Real) {
            return operand;
        }
        throw CompileError("Unary '-' requires int or real at line " + std::to_string(expr->line));
    }
    if (expr->op == "!") {
        if (operand->kind == Type::Kind::Bool) {
            return operand;
        }
        throw CompileError("Unary '!' requires bool at line " + std::to_string(expr->line));
    }
    throw CompileError("Invalid unary operator at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> ExprTypeInferer::InferBinary(const ExprPtr &expr, Env &env) {
    auto left = Infer(expr->left, env);
    auto right = Infer(expr->right, env);
    if (expr->op == "+" || expr->op == "-" || expr->op == "*" || expr->op == "/" || expr->op == "%") {
        if (left->kind == Type::Kind::Int && right->kind == Type::Kind::Int) {
            return left;
        }
        if (left->kind == Type::Kind::Real && right->kind == Type::Kind::Real) {
            return left;
        }
        if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
            (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
            return resolve_type_(TypeSpec{"real", 0, false});
        }
        throw CompileError("Arithmetic requires int or real at line " + std::to_string(expr->line));
    }
    if (expr->op == "==" || expr->op == "!=" || expr->op == "<" || expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
        if (left->kind == right->kind) {
            if (left->kind == Type::Kind::Int || left->kind == Type::Kind::Bool || left->kind == Type::Kind::Real) {
                return resolve_type_(TypeSpec{"bool", 0, false});
            }
        }
        if ((left->kind == Type::Kind::Real && right->kind == Type::Kind::Int) ||
            (left->kind == Type::Kind::Int && right->kind == Type::Kind::Real)) {
            return resolve_type_(TypeSpec{"bool", 0, false});
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

std::shared_ptr<Type> ExprTypeInferer::InferField(const ExprPtr &expr, Env &env) {
    auto base_type = Infer(expr->base, env);
    if (base_type->kind != Type::Kind::Struct) {
        throw CompileError("Field access on non-struct at line " + std::to_string(expr->line));
    }
    const StructInfo *info = lookup_struct_(base_type->name);
    if (!info) {
        throw CompileError("Unknown struct " + base_type->name + " at line " + std::to_string(expr->line));
    }
    auto it = info->field_map.find(expr->field);
    if (it == info->field_map.end()) {
        throw CompileError("Unknown field " + expr->field + " on struct " + base_type->name);
    }
    return it->second.type;
}

std::shared_ptr<Type> ExprTypeInferer::InferIndex(const ExprPtr &expr, Env &env) {
    auto base_type = Infer(expr->base, env);
    if (base_type->kind != Type::Kind::Array) {
        throw CompileError("Indexing non-array at line " + std::to_string(expr->line));
    }
    return base_type->element;
}

std::shared_ptr<Type> ExprTypeInferer::InferCall(const ExprPtr &expr, Env &env) {
    if (expr->base->kind == ExprKind::Var) {
        std::string name = expr->base->text;
        if (name == "print") {
            return resolve_type_(TypeSpec{"void", 0, true});
        }
        if (name == "sqrt") {
            return resolve_type_(TypeSpec{"real", 0, false});
        }
        const FunctionInfo *info = lookup_function_(name);
        if (!info) {
            throw CompileError("Unknown function " + name + " at line " + std::to_string(expr->line));
        }
        return info->return_type;
    }
    if (expr->base->kind == ExprKind::Field) {
        auto field = expr->base;
        auto base_type = Infer(field->base, env);
        if ((base_type->kind == Type::Kind::Array || base_type->kind == Type::Kind::String) && field->field == "length") {
            return resolve_type_(TypeSpec{"int", 0, false});
        }
        if (base_type->kind != Type::Kind::Struct) {
            throw CompileError("Method call on non-struct at line " + std::to_string(expr->line));
        }
        std::string method_name = base_type->name + "." + field->field;
        const FunctionInfo *info = lookup_function_(method_name);
        if (!info) {
            throw CompileError("Unknown method " + field->field + " on struct " + base_type->name);
        }
        return info->return_type;
    }
    throw CompileError("Unsupported call expression at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> ExprTypeInferer::InferNew(const ExprPtr &expr, Env &env) {
    (void)env;
    if (expr->new_size) {
        auto base = resolve_type_(expr->new_type);
        if (base->kind == Type::Kind::Array) {
            return base;
        }
        return std::make_shared<Type>(Type{Type::Kind::Array, "", base});
    }
    return resolve_type_(expr->new_type);
}
