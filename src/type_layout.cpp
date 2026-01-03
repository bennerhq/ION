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
        case TypeKind::Int:
        case TypeKind::Real:
        case TypeKind::Bool:
        case TypeKind::String:
        case TypeKind::Struct:
        case TypeKind::Array:
            return 8;
        case TypeKind::Void:
            return 0;
    }
    return 8;
}
