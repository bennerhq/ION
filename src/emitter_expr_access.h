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
#include <unordered_map>

#include "codegen_types.h"
#include "emitter_format.h"

class ExprAccessEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using ExprInferrer = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;
    using TypeSizer = std::function<int64_t(const std::shared_ptr<Type> &)>;
    using LoadEmitter = std::function<void(const std::shared_ptr<Type> &)>;
    using FunctionLookup = std::function<const FunctionInfo *(const std::string &)>;
    using StructLookup = std::function<const StructInfo *(const std::string &)>;
    using FormatterFactory = std::function<FormatEmitter &()>;
    using TypeChecker = std::function<void(const std::shared_ptr<Type> &,
                                           const std::shared_ptr<Type> &,
                                           int)>;

    ExprAccessEmitter(std::ostream &out,
                      ExprEmitter emit_expr,
                      ExprInferrer infer_expr,
                      TypeResolver resolve_type,
                      TypeSizer type_size,
                      LoadEmitter emit_load,
                      FunctionLookup lookup_function,
                      StructLookup lookup_struct,
                      FormatterFactory make_formatter,
                      TypeChecker type_check);

    std::shared_ptr<Type> EmitField(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> EmitIndex(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> EmitIndexAddress(const ExprPtr &expr, Env &env);
    std::shared_ptr<Type> EmitCall(const ExprPtr &expr, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
    ExprInferrer infer_expr_;
    TypeResolver resolve_type_;
    TypeSizer type_size_;
    LoadEmitter emit_load_;
    FunctionLookup lookup_function_;
    StructLookup lookup_struct_;
    FormatterFactory make_formatter_;
    TypeChecker type_check_;

};
