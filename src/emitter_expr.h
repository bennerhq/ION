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

class ExprOperatorEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using ExprInferrer = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;

    ExprOperatorEmitter(std::ostream &out,
                        ExprEmitter emit_expr,
                        ExprInferrer infer_expr,
                        TypeResolver resolve_type);

    std::shared_ptr<Type> EmitBinary(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
    ExprInferrer infer_expr_;
    TypeResolver resolve_type_;

    void EmitIntCompare(const std::string &op);
    void EmitFloatCompare(const std::string &op);
};

class ExprUnaryEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;

    ExprUnaryEmitter(std::ostream &out, ExprEmitter emit_expr);

    std::shared_ptr<Type> EmitUnary(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
};
