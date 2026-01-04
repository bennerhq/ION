// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#include "semantics.h"
#include "common.h"
#include "type_system.h" // For ResolveType and TypeLayout utils

// --- Symbol Lookup ---

std::optional<LookupResult>
FindIdentifier(const std::string &name, const Env &env,
               const std::unordered_map<std::string, StructInfo> &structs) {
  // std::cerr << "FindIdentifier: " << name << std::endl;
  auto it = env.locals.find(name);
  if (it != env.locals.end()) {
    return LookupResult{LookupResult::Kind::Local, &it->second, nullptr};
  }
  auto pit = env.params.find(name);
  if (pit != env.params.end()) {
    return LookupResult{LookupResult::Kind::Param, &pit->second, nullptr};
  }
  if (!env.current_struct.empty()) {
    auto sit = structs.find(env.current_struct);
    if (sit != structs.end()) {
      const StructInfo &info = sit->second;
      auto fit = info.field_map.find(name);
      if (fit != info.field_map.end()) {
        return LookupResult{LookupResult::Kind::Field, nullptr, &fit->second};
      }
    }
  }
  return std::nullopt;
}

// --- Struct Layout ---

void ComputeStructLayouts(
    const Program &program,
    std::unordered_map<std::string, StructInfo> &structs) {
  bool changed = true;
  while (changed) {
    changed = false;
    for (const auto &def : program.structs) {
      StructInfo &info = structs[def.name];
      // If already sized, skip, unless it's a derived struct waiting for parent
      if (info.size != 0) {
        continue; // Already processed
      }
      if (!def.parent.empty() && structs[def.parent].size == 0) {
        continue; // Parent not ready
      }

      int64_t offset = 0;
      if (!def.parent.empty()) {
        StructInfo &parent = structs[def.parent];
        offset = parent.size;
        info.fields = parent.fields;
      }
      for (const auto &field : def.fields) {
        FieldInfo finfo;
        finfo.name = field.second;
        finfo.type = ResolveType(field.first, structs);
        finfo.offset = offset;
        offset += GetTypeSize(finfo.type); // Using TypeSystem util
        info.fields.push_back(finfo);
      }
      info.size = Align8(offset);
      if (info.size == 0) {
        info.size = 8;
      }
      info.field_map.clear();
      for (const auto &field : info.fields) {
        info.field_map[field.name] = field;
      }
      changed = true;
    }
  }
  for (auto &entry : structs) {
    if (entry.second.size == 0) {
      throw CompileError("Struct layout failed for " + entry.first);
    }
  }
}

// --- Function Catalog ---

std::string MangleFunctionName(const std::string &function_name,
                               const std::string &owner_struct) {
  if (!owner_struct.empty()) {
    return "$" + owner_struct + "_" + function_name;
  }
  return "$" + function_name;
}

void BuildFunctionCatalog(
    const Program &program,
    std::unordered_map<std::string, StructInfo> &structs,
    std::unordered_map<std::string, FunctionInfo> &functions) {
  for (const auto &fn : program.functions) {
    FunctionInfo info;
    info.decl = const_cast<Function *>(&fn);
    info.return_type = ResolveType(fn.return_type, structs);
    for (const auto &param : fn.params) {
      info.params.push_back(ResolveType(param.first, structs));
    }
    info.wasm_name = MangleFunctionName(fn.name, "");
    functions[fn.name] = info;
  }
  for (const auto &def : program.structs) {
    for (const auto &method : def.methods) {
      FunctionInfo info;
      info.decl = const_cast<Function *>(&method);
      info.return_type = ResolveType(method.return_type, structs);

      // Add 'this' parameter
      info.params.push_back(ResolveType(TypeSpec{def.name, 0, false}, structs));

      for (const auto &param : method.params) {
        info.params.push_back(ResolveType(param.first, structs));
      }
      info.wasm_name = MangleFunctionName(method.name, def.name);
      functions[def.name + "." + method.name] = info;
      structs[def.name].methods[method.name] = info.decl;
    }
  }
}
