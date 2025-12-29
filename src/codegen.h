// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <string>
#include <unordered_set>

#include "ast.h"

std::string GenerateWasm(const Program &program, const std::unordered_set<std::string> &type_names);

