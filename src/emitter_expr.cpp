// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_expr.h"

#include "common.h"

ExprOperatorEmitter::ExprOperatorEmitter(std::ostream &out,
                                         ExprEmitter emit_expr,
                                         ExprInferrer infer_expr,
                                         TypeResolver resolve_type)
    : out_(out),
      emit_expr_(std::move(emit_expr)),
      infer_expr_(std::move(infer_expr)),
      resolve_type_(std::move(resolve_type)) {}

std::shared_ptr<Type> ExprOperatorEmitter::EmitBinary(const ExprPtr &expr, Env &env) {
    auto left = infer_expr_(expr->left, env);
    auto right = infer_expr_(expr->right, env);
    if (expr->op == "+" || expr->op == "-" || expr->op == "*" || expr->op == "/" || expr->op == "%") {
        if (left->kind == TypeKind::Int && right->kind == TypeKind::Int) {
            emit_expr_(expr->left, env);
            emit_expr_(expr->right, env);
            if (expr->op == "+") out_ << "    i64.add\n";
            if (expr->op == "-") out_ << "    i64.sub\n";
            if (expr->op == "*") out_ << "    i64.mul\n";
            if (expr->op == "/") out_ << "    i64.div_s\n";
            if (expr->op == "%") out_ << "    i64.rem_s\n";
            return left;
        }
        if (left->kind == TypeKind::Real && right->kind == TypeKind::Real) {
            emit_expr_(expr->left, env);
            emit_expr_(expr->right, env);
            if (expr->op == "+") out_ << "    f64.add\n";
            if (expr->op == "-") out_ << "    f64.sub\n";
            if (expr->op == "*") out_ << "    f64.mul\n";
            if (expr->op == "/") out_ << "    f64.div\n";
            return left;
        }
        if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
            (left->kind == TypeKind::Int && right->kind == TypeKind::Real)) {
            if (left->kind == TypeKind::Int) {
                emit_expr_(expr->left, env);
                out_ << "    f64.convert_i64_s\n";
                emit_expr_(expr->right, env);
            } else {
                emit_expr_(expr->left, env);
                emit_expr_(expr->right, env);
                out_ << "    f64.convert_i64_s\n";
            }
            if (expr->op == "+") out_ << "    f64.add\n";
            if (expr->op == "-") out_ << "    f64.sub\n";
            if (expr->op == "*") out_ << "    f64.mul\n";
            if (expr->op == "/") out_ << "    f64.div\n";
            return resolve_type_(TypeSpec{"real", 0, false});
        }
        throw CompileError("Arithmetic requires int or real at line " + std::to_string(expr->line));
    }
    if (expr->op == "==" || expr->op == "!=" || expr->op == "<" || expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
        if ((left->kind == TypeKind::Int || left->kind == TypeKind::Bool) && right->kind == left->kind) {
            emit_expr_(expr->left, env);
            emit_expr_(expr->right, env);
            EmitIntCompare(expr->op);
            return resolve_type_(TypeSpec{"bool", 0, false});
        }
        if (left->kind == TypeKind::Real && right->kind == TypeKind::Real) {
            emit_expr_(expr->left, env);
            emit_expr_(expr->right, env);
            EmitFloatCompare(expr->op);
            return resolve_type_(TypeSpec{"bool", 0, false});
        }
        if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
            (left->kind == TypeKind::Int && right->kind == TypeKind::Real)) {
            if (left->kind == TypeKind::Int) {
                emit_expr_(expr->left, env);
                out_ << "    f64.convert_i64_s\n";
                emit_expr_(expr->right, env);
            } else {
                emit_expr_(expr->left, env);
                emit_expr_(expr->right, env);
                out_ << "    f64.convert_i64_s\n";
            }
            EmitFloatCompare(expr->op);
            return resolve_type_(TypeSpec{"bool", 0, false});
        }
        throw CompileError("Comparison not supported for this type at line " + std::to_string(expr->line));
    }
    if (expr->op == "and" || expr->op == "or") {
        if (left->kind != TypeKind::Bool || right->kind != TypeKind::Bool) {
            throw CompileError("Logical operators require bool at line " + std::to_string(expr->line));
        }
        emit_expr_(expr->left, env);
        emit_expr_(expr->right, env);
        if (expr->op == "and") {
            out_ << "    i64.and\n";
        } else {
            out_ << "    i64.or\n";
        }
        return left;
    }
    throw CompileError("Unknown binary operator at line " + std::to_string(expr->line));
}

void ExprOperatorEmitter::EmitIntCompare(const std::string &op) {
    if (op == "==") out_ << "    i64.eq\n";
    if (op == "!=") out_ << "    i64.ne\n";
    if (op == "<") out_ << "    i64.lt_s\n";
    if (op == "<=") out_ << "    i64.le_s\n";
    if (op == ">") out_ << "    i64.gt_s\n";
    if (op == ">=") out_ << "    i64.ge_s\n";
    out_ << "    i64.extend_i32_u\n";
}

void ExprOperatorEmitter::EmitFloatCompare(const std::string &op) {
    if (op == "==") out_ << "    f64.eq\n";
    if (op == "!=") out_ << "    f64.ne\n";
    if (op == "<") out_ << "    f64.lt\n";
    if (op == "<=") out_ << "    f64.le\n";
    if (op == ">") out_ << "    f64.gt\n";
    if (op == ">=") out_ << "    f64.ge\n";
    out_ << "    i64.extend_i32_u\n";
}

ExprUnaryEmitter::ExprUnaryEmitter(std::ostream &out, ExprEmitter emit_expr)
    : out_(out), emit_expr_(std::move(emit_expr)) {}

std::shared_ptr<Type> ExprUnaryEmitter::EmitUnary(const ExprPtr &expr, Env &env) {
    auto operand = emit_expr_(expr->left, env);
    if (expr->op == "-") {
        if (operand->kind == TypeKind::Int) {
            out_ << "    i64.const -1\n";
            out_ << "    i64.mul\n";
            return operand;
        }
        if (operand->kind == TypeKind::Real) {
            out_ << "    f64.neg\n";
            return operand;
        }
        throw CompileError("Unary '-' requires int or real at line " + std::to_string(expr->line));
    }
    if (expr->op == "!") {
        if (operand->kind != TypeKind::Bool) {
            throw CompileError("Unary '!' requires bool at line " + std::to_string(expr->line));
        }
        out_ << "    i64.eqz\n";
        out_ << "    i64.extend_i32_u\n";
        return operand;
    }
    throw CompileError("Unknown unary operator at line " + std::to_string(expr->line));
}
