// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_expr_var.h"

#include "common.h"
#include "identifier_lookup.h"

ExprVarEmitter::ExprVarEmitter(std::ostream &out, LoadEmitter emit_load, StructLookup lookup_struct)
    : out_(out), emit_load_(std::move(emit_load)), lookup_struct_(std::move(lookup_struct)) {}

std::shared_ptr<Type> ExprVarEmitter::EmitVar(const ExprPtr &expr, Env &env) {
    auto result = IdentifierLookup::Find(expr->text, env, lookup_struct_);
    if (result) {
        if (result->kind == IdentifierLookup::Kind::Local || result->kind == IdentifierLookup::Kind::Param) {
            out_ << "    local.get " << result->local->wasm_name << "\n";
            return result->local->type;
        }
        out_ << "    local.get $this\n";
        out_ << "    i64.const " << result->field->offset << "\n";
        out_ << "    i64.add\n";
        emit_load_(result->field->type);
        return result->field->type;
    }
    throw CompileError("Unknown identifier " + expr->text + " at line " + std::to_string(expr->line));
}
