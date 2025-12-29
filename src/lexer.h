// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <string>
#include <vector>

#include "ast.h"

class Lexer {
public:
    explicit Lexer(const std::string &source);
    std::vector<Token> Tokenize();

private:
    std::string source_;

    static std::string StripComment(const std::string &line);
    static int CountIndent(const std::string &line);
    void LexLine(const std::string &line, int line_no, std::vector<Token> &tokens);
    static bool IsKeyword(const std::string &word);
};

