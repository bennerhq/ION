// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "codegen_types.h"

struct LookupResult {
  enum class Kind { Local, Param, Field };
  Kind kind;
  const LocalInfo *local = nullptr;
  const FieldInfo *field = nullptr;
};

// Symbol Lookup
std::optional<LookupResult>
FindIdentifier(const std::string &name, const Env &env,
               const std::unordered_map<std::string, StructInfo> &structs);

// Struct Layout
void ComputeStructLayouts(const Program &program,
                          std::unordered_map<std::string, StructInfo> &structs);

// Function Catalog
void BuildFunctionCatalog(
    const Program &program,
    std::unordered_map<std::string, StructInfo> &structs,
    std::unordered_map<std::string, FunctionInfo> &functions);
std::string MangleFunctionName(const std::string &function_name,
                               const std::string &owner_struct);
