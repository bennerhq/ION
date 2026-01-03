// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"

struct FieldInfo {
  std::string name;
  std::shared_ptr<Type> type;
  int64_t offset = 0;
};

struct StructInfo {
  std::string name;
  std::string parent;
  std::vector<FieldInfo> fields;
  std::unordered_map<std::string, FieldInfo> field_map;
  std::unordered_map<std::string, Function *> methods;
  int64_t size = 0;
};

struct FunctionInfo {
  Function *decl = nullptr;
  std::shared_ptr<Type> return_type;
  std::vector<std::shared_ptr<Type>> params;
  std::string wasm_name;
};

struct LocalInfo {
  std::string wasm_name;
  std::shared_ptr<Type> type;
};

struct Env {
  std::unordered_map<std::string, LocalInfo> locals;
  std::unordered_map<std::string, LocalInfo> params;
  std::string current_struct;
};
