// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>

void EmitRuntime(std::ostream &out,
                 const std::unordered_map<std::string, int64_t> &string_offsets);

