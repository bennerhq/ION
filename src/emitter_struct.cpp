// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_struct.h"

StructEmitter::StructEmitter(std::ostream &out,
                             const std::unordered_map<std::string, StructInfo> &structs,
                             InitEmitter emit_init)
    : out_(out), structs_(structs), emit_init_(std::move(emit_init)) {}

void StructEmitter::EmitStructInit(const StructDef &def) {
    const auto &info = structs_.at(def.name);
    out_ << "  (func $init_" << def.name << " (param $ptr i64) (local $tmp0 i64)\n";
    for (const auto &field : info.fields) {
        if (field.type->kind != TypeKind::Struct) {
            continue;
        }
        int64_t child_size = structs_.at(field.type->name).size;
        out_ << "    i64.const " << child_size << "\n";
        out_ << "    call $alloc\n";
        out_ << "    local.set $tmp0\n";
        out_ << "    local.get $ptr\n";
        out_ << "    i64.const " << field.offset << "\n";
        out_ << "    i64.add\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    local.get $tmp0\n";
        out_ << "    i64.store\n";
        out_ << "    local.get $tmp0\n";
        emit_init_(field.type->name);
    }
    out_ << "  )\n";
}
