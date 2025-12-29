// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <functional>
#include <memory>
#include <ostream>

#include "codegen_types.h"

class ConditionEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;

    ConditionEmitter(std::ostream &out, ExprEmitter emit_expr);

    void EmitCondition(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
};
