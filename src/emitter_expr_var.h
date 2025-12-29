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
#include <string>

#include "codegen_types.h"

class ExprVarEmitter {
public:
    using LoadEmitter = std::function<void(const std::shared_ptr<Type> &)>;
    using StructLookup = std::function<const StructInfo *(const std::string &)>;

    ExprVarEmitter(std::ostream &out, LoadEmitter emit_load, StructLookup lookup_struct);

    std::shared_ptr<Type> EmitVar(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    LoadEmitter emit_load_;
    StructLookup lookup_struct_;
};
