// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_module.h"

#include <sstream>

#include "common.h"
#include "emitter_runtime.h"

ModuleEmitter::ModuleEmitter(std::ostream &out,
                             const Program &program,
                             const StringLiteralTable &strings,
                             FunctionEmitter emit_function,
                             StructInitEmitter emit_struct_init,
                             FunctionInfoGetter get_function_info)
    : out_(out),
      program_(program),
      strings_(strings),
      emit_function_(std::move(emit_function)),
      emit_struct_init_(std::move(emit_struct_init)),
      get_function_info_(std::move(get_function_info)) {}

void ModuleEmitter::EmitModule() {
    out_ << "(module\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"fd_write\" (func $fd_write (param i32 i32 i32 i32) (result i32)))\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"args_sizes_get\" (func $args_sizes_get (param i32 i32) (result i32)))\n";
    out_ << "  (import \"wasi_snapshot_preview1\" \"args_get\" (func $args_get (param i32 i32) (result i32)))\n";
    out_ << "  (memory (export \"memory\") 1)\n";
    out_ << "  (global $heap (mut i64) (i64.const " << strings_.HeapStart() << "))\n";
    EmitDataSegments();
    RuntimeEmitter runtime(out_, strings_.Offsets());
    runtime.Emit();
    for (const auto &fn : program_.functions) {
        emit_function_(fn, "");
    }
    for (const auto &def : program_.structs) {
        emit_struct_init_(def);
        for (const auto &method : def.methods) {
            emit_function_(method, def.name);
        }
    }
    EmitStart();
    out_ << ")\n";
}

void ModuleEmitter::EmitDataSegments() {
    for (const auto &seg : strings_.Segments()) {
        out_ << "  (data (i32.const " << seg.first << ") \"" << EscapeBytes(seg.second) << "\")\n";
    }
}

void ModuleEmitter::EmitStart() {
    const FunctionInfo *info = get_function_info_("main");
    if (!info) {
        throw CompileError("No main function defined");
    }
    out_ << "  (func $_start (export \"_start\")\n";
    if (info->params.empty()) {
        out_ << "    call " << info->wasm_name << "\n";
    } else if (info->params.size() == 1 &&
               info->params[0]->kind == TypeKind::Array &&
               info->params[0]->element &&
               info->params[0]->element->kind == TypeKind::String) {
        out_ << "    call $build_args\n";
        out_ << "    call " << info->wasm_name << "\n";
    } else {
        throw CompileError("main must be void main() or void main(string[] args)");
    }
    if (info->return_type->kind != TypeKind::Void) {
        out_ << "    drop\n";
    }
    out_ << "  )\n";
}

std::string ModuleEmitter::EscapeBytes(const std::string &bytes) {
    std::ostringstream ss;
    for (unsigned char c : bytes) {
        if (c >= 32 && c <= 126 && c != '"' && c != '\\') {
            ss << c;
        } else {
            ss << "\\";
            const char *hex = "0123456789ABCDEF";
            ss << hex[(c >> 4) & 0xF] << hex[c & 0xF];
        }
    }
    return ss.str();
}
