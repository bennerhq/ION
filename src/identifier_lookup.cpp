// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "identifier_lookup.h"

std::optional<IdentifierLookup::Result> IdentifierLookup::Find(const std::string &name,
                                                                const Env &env,
                                                                const StructLookup &lookup_struct) {
    auto it = env.locals.find(name);
    if (it != env.locals.end()) {
        return Result{Kind::Local, &it->second, nullptr};
    }
    auto pit = env.params.find(name);
    if (pit != env.params.end()) {
        return Result{Kind::Param, &pit->second, nullptr};
    }
    if (!env.current_struct.empty()) {
        const StructInfo *info = lookup_struct(env.current_struct);
        if (info) {
            auto fit = info->field_map.find(name);
            if (fit != info->field_map.end()) {
                return Result{Kind::Field, nullptr, &fit->second};
            }
        }
    }
    return std::nullopt;
}
