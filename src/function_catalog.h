// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <functional>
#include <string>
#include <unordered_map>

#include "codegen_types.h"

class FunctionCatalog {
public:
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;

    FunctionCatalog(const Program &program,
                    std::unordered_map<std::string, StructInfo> &structs,
                    std::unordered_map<std::string, FunctionInfo> &functions,
                    TypeResolver resolve_type);

    void Build();
    std::string MangleFunctionName(const Function &fn, const std::string &owner) const;

private:
    const Program &program_;
    std::unordered_map<std::string, StructInfo> &structs_;
    std::unordered_map<std::string, FunctionInfo> &functions_;
    TypeResolver resolve_type_;
};
