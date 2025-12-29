// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <functional>
#include <ostream>
#include <string>
#include <unordered_map>

#include "codegen_types.h"

class StructEmitter {
public:
    using InitEmitter = std::function<void(const std::string &)>;

    StructEmitter(std::ostream &out,
                  const std::unordered_map<std::string, StructInfo> &structs,
                  InitEmitter emit_init);

    void EmitStructInit(const StructDef &def);

private:
    std::ostream &out_;
    const std::unordered_map<std::string, StructInfo> &structs_;
    InitEmitter emit_init_;
};
