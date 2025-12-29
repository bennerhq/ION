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

class ExprNewEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;
    using TypeSizer = std::function<int64_t(const std::shared_ptr<Type> &)>;
    using StructLookup = std::function<const StructInfo *(const std::string &)>;

    ExprNewEmitter(std::ostream &out,
                   ExprEmitter emit_expr,
                   TypeResolver resolve_type,
                   TypeSizer type_size,
                   StructLookup lookup_struct);

    std::shared_ptr<Type> EmitNew(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
    TypeResolver resolve_type_;
    TypeSizer type_size_;
    StructLookup lookup_struct_;
};
