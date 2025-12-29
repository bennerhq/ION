// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <functional>
#include <optional>
#include <string>

#include "codegen_types.h"

class IdentifierLookup {
public:
    enum class Kind { Local, Param, Field };

    struct Result {
        Kind kind;
        const LocalInfo *local = nullptr;
        const FieldInfo *field = nullptr;
    };

    using StructLookup = std::function<const StructInfo *(const std::string &)>;

    static std::optional<Result> Find(const std::string &name,
                                      const Env &env,
                                      const StructLookup &lookup_struct);
};
