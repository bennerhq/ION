// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_expr_access.h"

#include "common.h"

ExprAccessEmitter::ExprAccessEmitter(std::ostream &out,
                                     ExprEmitter emit_expr,
                                     ExprInferrer infer_expr,
                                     TypeResolver resolve_type,
                                     TypeSizer type_size,
                                     LoadEmitter emit_load,
                                     FunctionLookup lookup_function,
                                     StructLookup lookup_struct,
                                     FormatterFactory make_formatter,
                                     TypeChecker type_check)
    : out_(out),
      emit_expr_(std::move(emit_expr)),
      infer_expr_(std::move(infer_expr)),
      resolve_type_(std::move(resolve_type)),
      type_size_(std::move(type_size)),
      emit_load_(std::move(emit_load)),
      lookup_function_(std::move(lookup_function)),
      lookup_struct_(std::move(lookup_struct)),
      make_formatter_(std::move(make_formatter)),
      type_check_(std::move(type_check)) {}

std::shared_ptr<Type> ExprAccessEmitter::EmitField(const ExprPtr &expr, Env &env) {
    auto base_type = emit_expr_(expr->base, env);
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
    out_ << "    i64.const " << it->second.offset << "\n";
    out_ << "    i64.add\n";
    emit_load_(it->second.type);
    return it->second.type;
}

std::shared_ptr<Type> ExprAccessEmitter::EmitIndex(const ExprPtr &expr, Env &env) {
    auto element_type = EmitIndexAddress(expr, env);
    emit_load_(element_type);
    return element_type;
}

std::shared_ptr<Type> ExprAccessEmitter::EmitCall(const ExprPtr &expr, Env &env) {
    if (expr->base->kind == ExprKind::Var) {
        std::string name = expr->base->text;
        if (name == "print") {
            if (expr->args.empty()) {
                throw CompileError("print expects at least 1 argument at line " + std::to_string(expr->line));
            }
            FormatEmitter &formatter = make_formatter_();
            if (expr->args[0]->kind == ExprKind::StringLit) {
                bool uses_format = expr->args.size() > 1 || formatter.NeedsFormat(expr->args[0]->text);
                if (uses_format) {
                    formatter.EmitFormattedPrint(expr->args[0]->text, expr->args, env, expr->line);
                    return resolve_type_(TypeSpec{"void", 0, true});
                }
            }
            if (expr->args.size() > 1 && expr->args[0]->kind != ExprKind::StringLit) {
                formatter.EmitRuntimeFormatCall(expr->args, env, expr->line);
                return resolve_type_(TypeSpec{"void", 0, true});
            }
            if (expr->args.size() != 1) {
                throw CompileError("print expects 1 argument or a format string at line " + std::to_string(expr->line));
            }
            auto arg_type = emit_expr_(expr->args[0], env);
            if (arg_type->kind == Type::Kind::Int) {
                out_ << "    call $print_i64\n";
                return resolve_type_(TypeSpec{"void", 0, true});
            }
            if (arg_type->kind == Type::Kind::Bool) {
                out_ << "    call $print_bool\n";
                return resolve_type_(TypeSpec{"void", 0, true});
            }
            if (arg_type->kind == Type::Kind::String) {
                out_ << "    call $print_string\n";
                return resolve_type_(TypeSpec{"void", 0, true});
            }
            if (arg_type->kind == Type::Kind::Real) {
                out_ << "    call $print_f64\n";
                return resolve_type_(TypeSpec{"void", 0, true});
            }
            throw CompileError("print only supports int, bool, real, string at line " + std::to_string(expr->line));
        }
        if (name == "sqrt") {
            if (expr->args.size() != 1) {
                throw CompileError("sqrt expects 1 argument at line " + std::to_string(expr->line));
            }
            auto arg_type = emit_expr_(expr->args[0], env);
            if (arg_type->kind != Type::Kind::Real) {
                throw CompileError("sqrt expects real at line " + std::to_string(expr->line));
            }
            out_ << "    f64.sqrt\n";
            return arg_type;
        }
        const FunctionInfo *info = lookup_function_(name);
        if (!info) {
            throw CompileError("Unknown function " + name + " at line " + std::to_string(expr->line));
        }
        if (expr->args.size() != info->params.size()) {
            throw CompileError("Argument count mismatch for " + name);
        }
        for (size_t i = 0; i < expr->args.size(); ++i) {
            auto arg_type = emit_expr_(expr->args[i], env);
            type_check_(info->params[i], arg_type, expr->line);
        }
        out_ << "    call " << info->wasm_name << "\n";
        return info->return_type;
    }
    if (expr->base->kind == ExprKind::Field) {
        auto field = expr->base;
        auto base_type = emit_expr_(field->base, env);
        if (base_type->kind == Type::Kind::Array && field->field == "length") {
            if (!expr->args.empty()) {
                throw CompileError("length() takes no args at line " + std::to_string(expr->line));
            }
            out_ << "    i32.wrap_i64\n";
            out_ << "    i64.load\n";
            return resolve_type_(TypeSpec{"int", 0, false});
        }
        if (base_type->kind == Type::Kind::String && field->field == "length") {
            if (!expr->args.empty()) {
                throw CompileError("length() takes no args at line " + std::to_string(expr->line));
            }
            out_ << "    i32.wrap_i64\n";
            out_ << "    i64.load\n";
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
        if (expr->args.size() + 1 != info->params.size()) {
            throw CompileError("Argument count mismatch for method " + field->field);
        }
        for (size_t i = 0; i < expr->args.size(); ++i) {
            auto arg_type = emit_expr_(expr->args[i], env);
            type_check_(info->params[i + 1], arg_type, expr->line);
        }
        out_ << "    call " << info->wasm_name << "\n";
        return info->return_type;
    }
    throw CompileError("Unsupported call expression at line " + std::to_string(expr->line));
}

std::shared_ptr<Type> ExprAccessEmitter::EmitIndexAddress(const ExprPtr &expr, Env &env) {
    auto base_type = emit_expr_(expr->base, env);
    if (base_type->kind != Type::Kind::Array) {
        throw CompileError("Indexing non-array at line " + std::to_string(expr->line));
    }
    out_ << "    local.set $tmp0\n";
    auto index_type = emit_expr_(expr->left, env);
    if (index_type->kind != Type::Kind::Int) {
        throw CompileError("Array index must be int at line " + std::to_string(expr->line));
    }
    out_ << "    local.set $tmp1\n";
    int64_t elem_size = type_size_(base_type->element);
    out_ << "    local.get $tmp0\n";
    out_ << "    i64.const 8\n";
    out_ << "    i64.add\n";
    out_ << "    local.get $tmp1\n";
    out_ << "    i64.const " << elem_size << "\n";
    out_ << "    i64.mul\n";
    out_ << "    i64.add\n";
    return base_type->element;
}
