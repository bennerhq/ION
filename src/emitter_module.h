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

#include "codegen_types.h"
#include "string_table.h"

class ModuleEmitter {
public:
    using FunctionEmitter = std::function<void(const Function &, const std::string &)>;
    using StructInitEmitter = std::function<void(const StructDef &)>;
    using FunctionInfoGetter = std::function<const FunctionInfo *(const std::string &)>;

    ModuleEmitter(std::ostream &out,
                  const Program &program,
                  const StringLiteralTable &strings,
                  FunctionEmitter emit_function,
                  StructInitEmitter emit_struct_init,
                  FunctionInfoGetter get_function_info);

    void EmitModule();

private:
    std::ostream &out_;
    const Program &program_;
    const StringLiteralTable &strings_;
    FunctionEmitter emit_function_;
    StructInitEmitter emit_struct_init_;
    FunctionInfoGetter get_function_info_;

    void EmitDataSegments();
    void EmitStart();
    std::string EscapeBytes(const std::string &bytes);
};
