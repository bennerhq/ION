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
#include <vector>

#include "codegen_types.h"

class FormatEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using ExprInferrer = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;

    FormatEmitter(std::ostream &out,
                  const std::unordered_map<std::string, int64_t> &string_offsets,
                  ExprEmitter emit_expr,
                  ExprInferrer infer_expr,
                  TypeResolver resolve_type);

    bool NeedsFormat(const std::string &format) const;
    void EmitFormattedPrint(const std::string &format, const std::vector<ExprPtr> &args, Env &env, int line);
    void EmitRuntimeFormatCall(const std::vector<ExprPtr> &args, Env &env, int line);

private:
    std::ostream &out_;
    const std::unordered_map<std::string, int64_t> &string_offsets_;
    ExprEmitter emit_expr_;
    ExprInferrer infer_expr_;
    TypeResolver resolve_type_;
};
