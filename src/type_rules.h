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

class TypeRules {
public:
    explicit TypeRules(const std::unordered_map<std::string, StructInfo> &structs);

    void RequireSameType(const std::shared_ptr<Type> &expected,
                         const std::shared_ptr<Type> &actual,
                         int line) const;
    bool IsAssignable(const std::shared_ptr<Type> &expected,
                      const std::shared_ptr<Type> &actual) const;

private:
    const std::unordered_map<std::string, StructInfo> &structs_;
};
