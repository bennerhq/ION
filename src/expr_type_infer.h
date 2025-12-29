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
#include <string>
#include <unordered_map>

#include "codegen_types.h"

class ExprTypeInferer {
public:
    using ExprInfer = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;
    using FunctionLookup = std::function<const FunctionInfo *(const std::string &)>;
    using StructLookup = std::function<const StructInfo *(const std::string &)>;

    ExprTypeInferer(TypeResolver resolve_type,
                    FunctionLookup lookup_function,
                    StructLookup lookup_struct);

    std::shared_ptr<Type> Infer(const ExprPtr &expr, Env &env);

private:
    TypeResolver resolve_type_;
    FunctionLookup lookup_function_;
    StructLookup lookup_struct_;

    std::shared_ptr<Type> InferVar(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferUnary(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferBinary(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferField(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferIndex(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferCall(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> InferNew(const ExprPtr &expr, Env &env);
};
