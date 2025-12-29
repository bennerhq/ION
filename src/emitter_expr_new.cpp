// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_expr_new.h"

#include "common.h"

ExprNewEmitter::ExprNewEmitter(std::ostream &out,
                               ExprEmitter emit_expr,
                               TypeResolver resolve_type,
                               TypeSizer type_size,
                               StructLookup lookup_struct)
    : out_(out),
      emit_expr_(std::move(emit_expr)),
      resolve_type_(std::move(resolve_type)),
      type_size_(std::move(type_size)),
      lookup_struct_(std::move(lookup_struct)) {}

std::shared_ptr<Type> ExprNewEmitter::EmitNew(const ExprPtr &expr, Env &env) {
    auto type = resolve_type_(expr->new_type);
    if (expr->new_size) {
        if (type->kind != Type::Kind::Array) {
            type = std::make_shared<Type>(Type{Type::Kind::Array, "", type});
        }
        auto size_type = emit_expr_(expr->new_size, env);
        if (size_type->kind != Type::Kind::Int) {
            throw CompileError("Array size must be int at line " + std::to_string(expr->line));
        }
        int64_t elem_size = type_size_(type->element);
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
    const StructInfo *info = lookup_struct_(type->name);
    if (!info) {
        throw CompileError("Unknown struct " + type->name + " at line " + std::to_string(expr->line));
    }
    int64_t size = info->size;
    out_ << "    i64.const " << size << "\n";
    out_ << "    call $alloc\n";
    out_ << "    local.set $tmp0\n";
    out_ << "    local.get $tmp0\n";
    out_ << "    call $init_" << type->name << "\n";
    out_ << "    local.get $tmp0\n";
    return type;
}
