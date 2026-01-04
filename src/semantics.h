// THE BEER LICENSE (with extra fizz)
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
