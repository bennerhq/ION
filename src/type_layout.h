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

#include "codegen_types.h"

class TypeLayout {
public:
    static int64_t Align8(int64_t value);
    static int64_t SizeOf(const std::shared_ptr<Type> &type);
};
