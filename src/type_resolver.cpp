// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "type_resolver.h"

#include "common.h"

TypeResolver::TypeResolver(const Program &program, std::unordered_map<std::string, StructInfo> &structs)
    : program_(program), structs_(structs) {}

void TypeResolver::InitStructs() {
    for (const auto &def : program_.structs) {
        StructInfo info;
        info.name = def.name;
        info.parent = def.parent;
        structs_[def.name] = info;
    }
}

std::shared_ptr<Type> TypeResolver::Resolve(const TypeSpec &spec) const {
    if (spec.is_void) {
        return std::make_shared<Type>(Type{TypeKind::Void, "void", nullptr});
    }
    std::shared_ptr<Type> base;
    if (spec.name == "int") {
        base = std::make_shared<Type>(Type{TypeKind::Int, "int", nullptr});
    } else if (spec.name == "real") {
        base = std::make_shared<Type>(Type{TypeKind::Real, "real", nullptr});
    } else if (spec.name == "bool") {
        base = std::make_shared<Type>(Type{TypeKind::Bool, "bool", nullptr});
    } else if (spec.name == "string") {
        base = std::make_shared<Type>(Type{TypeKind::String, "string", nullptr});
    } else {
        if (structs_.count(spec.name) == 0) {
            throw CompileError("Unknown type '" + spec.name + "'");
        }
        base = std::make_shared<Type>(Type{TypeKind::Struct, spec.name, nullptr});
    }
    for (int i = 0; i < spec.array_depth; ++i) {
        base = std::make_shared<Type>(Type{TypeKind::Array, "", base});
    }
    return base;
}
