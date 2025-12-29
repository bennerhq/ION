// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "struct_layout.h"

#include "common.h"
#include "type_layout.h"

StructLayout::StructLayout(const Program &program, const TypeResolver &resolver,
                           std::unordered_map<std::string, StructInfo> &structs)
    : program_(program), resolver_(resolver), structs_(structs) {}

void StructLayout::ComputeLayouts() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto &def : program_.structs) {
            StructInfo &info = structs_[def.name];
            if (info.size != 0 || (!def.parent.empty() && structs_[def.parent].size == 0)) {
                continue;
            }
            int64_t offset = 0;
            if (!def.parent.empty()) {
                StructInfo &parent = structs_[def.parent];
                offset = parent.size;
                info.fields = parent.fields;
            }
            for (const auto &field : def.fields) {
                FieldInfo finfo;
                finfo.name = field.second;
                finfo.type = resolver_.Resolve(field.first);
                finfo.offset = offset;
                offset += TypeLayout::SizeOf(finfo.type);
                info.fields.push_back(finfo);
            }
            info.size = TypeLayout::Align8(offset);
            if (info.size == 0) {
                info.size = 8;
            }
            info.field_map.clear();
            for (const auto &field : info.fields) {
                info.field_map[field.name] = field;
            }
            changed = true;
        }
    }
    for (auto &entry : structs_) {
        if (entry.second.size == 0) {
            throw CompileError("Struct layout failed for " + entry.first);
        }
    }
}
