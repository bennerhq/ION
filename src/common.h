// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#pragma once
#include <stdexcept>
#include <string>

struct CompileError : public std::runtime_error {
    explicit CompileError(const std::string &msg) : std::runtime_error(msg) {}
};

