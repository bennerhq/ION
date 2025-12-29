// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "function_catalog.h"

FunctionCatalog::FunctionCatalog(const Program &program,
                                 std::unordered_map<std::string, StructInfo> &structs,
                                 std::unordered_map<std::string, FunctionInfo> &functions,
                                 TypeResolver resolve_type)
    : program_(program),
      structs_(structs),
      functions_(functions),
      resolve_type_(std::move(resolve_type)) {}

void FunctionCatalog::Build() {
    for (const auto &fn : program_.functions) {
        FunctionInfo info;
        info.decl = const_cast<Function *>(&fn);
        info.return_type = resolve_type_(fn.return_type);
        for (const auto &param : fn.params) {
            info.params.push_back(resolve_type_(param.first));
        }
        info.wasm_name = MangleFunctionName(fn, "");
        functions_[fn.name] = info;
    }
    for (const auto &def : program_.structs) {
        for (const auto &method : def.methods) {
            FunctionInfo info;
            info.decl = const_cast<Function *>(&method);
            info.return_type = resolve_type_(method.return_type);
            info.params.push_back(resolve_type_(TypeSpec{def.name, 0, false}));
            for (const auto &param : method.params) {
                info.params.push_back(resolve_type_(param.first));
            }
            info.wasm_name = MangleFunctionName(method, def.name);
            functions_[def.name + "." + method.name] = info;
            structs_[def.name].methods[method.name] = info.decl;
        }
    }
}

std::string FunctionCatalog::MangleFunctionName(const Function &fn, const std::string &owner) const {
    if (!owner.empty()) {
        return "$" + owner + "_" + fn.name;
    }
    return "$" + fn.name;
}
