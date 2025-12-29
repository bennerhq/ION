// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <unordered_set>
#include <vector>

#include "ast.h"

class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens,
                    const std::unordered_set<std::string> &extra_types = {});
    Program ParseProgram();
    const std::unordered_set<std::string> &TypeNames() const;

private:
    const std::vector<Token> &tokens_;
    size_t current_ = 0;
    std::unordered_set<std::string> type_names_;

    void PreScanStructNames();
    bool IsStructDeclLine();
    ImportDecl ParseImport();
    StructDef ParseStructDecl();
    bool IsFunctionDeclLine();
    Function ParseFunctionDecl(bool is_method, const std::string &owner);
    std::pair<TypeSpec, std::string> ParseStructField();
    std::vector<StmtPtr> ParseBlock();
    StmtPtr ParseStatement();
    StmtPtr ParseIf();
    StmtPtr ParseWhile();
    TypeSpec ParseReturnType();
    TypeSpec ParseType();
    ExprPtr ParseExpression();
    ExprPtr ParseLogicalOr();
    ExprPtr ParseLogicalAnd();
    ExprPtr ParseEquality();
    ExprPtr ParseRelational();
    ExprPtr ParseAdditive();
    ExprPtr ParseMultiplicative();
    ExprPtr ParseUnary();
    ExprPtr ParsePostfix();
    ExprPtr ParseTerm();
    ExprPtr MakeBinary(const std::string &op, ExprPtr left, ExprPtr right);
    ExprPtr MakeUnary(const std::string &op, ExprPtr operand);
    bool IsTypeStart();
    bool Match(TokenType type);
    bool MatchKeyword(const std::string &kw);
    Token Consume(TokenType type, const std::string &msg);
    bool Check(TokenType type) const;
    Token Advance();
    bool IsAtEnd() const;
    Token Peek() const;
    Token Previous() const;
    Token PeekNext() const;
};
