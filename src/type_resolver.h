// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <memory>
#include <string>
#include <unordered_map>

#include "codegen_types.h"

class TypeResolver {
public:
    TypeResolver(const Program &program, std::unordered_map<std::string, StructInfo> &structs);
    void InitStructs();
    std::shared_ptr<Type> Resolve(const TypeSpec &spec) const;

private:
    const Program &program_;
    std::unordered_map<std::string, StructInfo> &structs_;
};
