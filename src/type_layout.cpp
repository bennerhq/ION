// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "type_layout.h"

int64_t TypeLayout::Align8(int64_t value) {
    return (value + 7) & ~static_cast<int64_t>(7);
}

int64_t TypeLayout::SizeOf(const std::shared_ptr<Type> &type) {
    switch (type->kind) {
        case Type::Kind::Int:
        case Type::Kind::Real:
        case Type::Kind::Bool:
        case Type::Kind::String:
        case Type::Kind::Struct:
        case Type::Kind::Array:
            return 8;
        case Type::Kind::Void:
            return 0;
    }
    return 8;
}
