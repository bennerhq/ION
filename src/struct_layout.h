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
#include <unordered_map>

#include "codegen_types.h"
#include "type_resolver.h"

class StructLayout {
public:
    StructLayout(const Program &program, const TypeResolver &resolver,
                 std::unordered_map<std::string, StructInfo> &structs);
    void ComputeLayouts();

private:
    const Program &program_;
    const TypeResolver &resolver_;
    std::unordered_map<std::string, StructInfo> &structs_;

    static int64_t Align8(int64_t value);
    static int64_t TypeSize(const std::shared_ptr<Type> &type);
};
