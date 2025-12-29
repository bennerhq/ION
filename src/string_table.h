// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.h"

class StringLiteralTable {
public:
    explicit StringLiteralTable(int64_t base_cursor = 4096);
    void Build(const Program &program);
    const std::unordered_map<std::string, int64_t> &Offsets() const;
    const std::vector<std::pair<int64_t, std::string>> &Segments() const;
    int64_t HeapStart() const;

private:
    int64_t base_cursor_;
    int64_t data_cursor_;
    int64_t heap_start_;
    std::unordered_map<std::string, int64_t> string_offsets_;
    std::vector<std::pair<int64_t, std::string>> data_segments_;

    void Reset();
    void AddBuiltins();
    void CollectStrings(const StmtPtr &stmt);
    void CollectStrings(const ExprPtr &expr);
    int64_t AddStringLiteral(const std::string &value);
    void AddFormatLiterals(const std::string &format);
    static bool NeedsFormat(const std::string &format);
    static int64_t Align8(int64_t value);
};
