// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_condition.h"

#include "common.h"

ConditionEmitter::ConditionEmitter(std::ostream &out, ExprEmitter emit_expr)
    : out_(out), emit_expr_(std::move(emit_expr)) {}

void ConditionEmitter::EmitCondition(const ExprPtr &expr, Env &env) {
    auto type = emit_expr_(expr, env);
    if (type->kind != TypeKind::Bool) {
        throw CompileError("Condition must be bool at line " + std::to_string(expr->line));
    }
    out_ << "    i32.wrap_i64\n";
}
