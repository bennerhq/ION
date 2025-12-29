// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "type_rules.h"

#include "common.h"

TypeRules::TypeRules(const std::unordered_map<std::string, StructInfo> &structs)
    : structs_(structs) {}

void TypeRules::RequireSameType(const std::shared_ptr<Type> &expected,
                                const std::shared_ptr<Type> &actual,
                                int line) const {
    if (!IsAssignable(expected, actual)) {
        throw CompileError("Type mismatch at line " + std::to_string(line));
    }
}

bool TypeRules::IsAssignable(const std::shared_ptr<Type> &expected,
                             const std::shared_ptr<Type> &actual) const {
    if (expected->kind != actual->kind) {
        return false;
    }
    if (expected->kind == Type::Kind::Array) {
        return IsAssignable(expected->element, actual->element);
    }
    if (expected->kind == Type::Kind::Struct) {
        if (expected->name == actual->name) {
            return true;
        }
        std::string current = actual->name;
        while (!current.empty()) {
            auto it = structs_.find(current);
            if (it == structs_.end()) {
                break;
            }
            if (it->second.parent == expected->name) {
                return true;
            }
            current = it->second.parent;
        }
        return false;
    }
    return true;
}
