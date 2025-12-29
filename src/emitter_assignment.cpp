// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_assignment.h"

#include "common.h"
#include "identifier_lookup.h"

AssignmentEmitter::AssignmentEmitter(std::ostream &out,
                                     ExprEmitter emit_expr,
                                     TypeSizer type_size,
                                     TypeChecker type_check,
                                     StructLookup lookup_struct,
                                     IndexAddressEmitter emit_index_address)
    : out_(out),
      emit_expr_(std::move(emit_expr)),
      type_size_(std::move(type_size)),
      type_check_(std::move(type_check)),
      lookup_struct_(std::move(lookup_struct)),
      emit_index_address_(std::move(emit_index_address)) {}

void AssignmentEmitter::EmitAssignment(const ExprPtr &target, const ExprPtr &value, Env &env) {
    if (target->kind == ExprKind::Var) {
        auto result = IdentifierLookup::Find(target->text, env, lookup_struct_);
        if (!result) {
            throw CompileError("Unknown variable " + target->text + " at line " + std::to_string(target->line));
        }
        if (result->kind == IdentifierLookup::Kind::Local ||
            result->kind == IdentifierLookup::Kind::Param) {
            auto rhs_type = emit_expr_(value, env);
            type_check_(result->local->type, rhs_type, value->line);
            out_ << "    local.set " << result->local->wasm_name << "\n";
            return;
        }
        out_ << "    local.get $this\n";
        out_ << "    i64.const " << result->field->offset << "\n";
        out_ << "    i64.add\n";
        out_ << "    local.set $tmp2\n";
        auto rhs_type = emit_expr_(value, env);
        type_check_(result->field->type, rhs_type, value->line);
        EmitStore(rhs_type);
        return;
    }
    auto addr_type = EmitAddress(target, env);
    out_ << "    local.set $tmp2\n";
    auto rhs_type = emit_expr_(value, env);
    type_check_(addr_type, rhs_type, value->line);
    EmitStore(rhs_type);
}

std::shared_ptr<Type> AssignmentEmitter::EmitAddress(const ExprPtr &expr, Env &env) {
    if (expr->kind == ExprKind::Field) {
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
        return it->second.type;
    }
    if (expr->kind == ExprKind::Index) {
        return emit_index_address_(expr, env);
    }
    throw CompileError("Invalid assignment target at line " + std::to_string(expr->line));
}

void AssignmentEmitter::EmitStore(const std::shared_ptr<Type> &type) {
    if (type->kind == Type::Kind::Real) {
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
