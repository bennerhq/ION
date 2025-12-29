// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "parser.h"

Parser::Parser(const std::vector<Token> &tokens, const std::unordered_set<std::string> &extra_types)
    : tokens_(tokens),
      type_names_({"int", "real", "bool", "string"}) {
    type_names_.insert(extra_types.begin(), extra_types.end());
    PreScanStructNames();
}

Program Parser::ParseProgram() {
    Program program;
    while (!Check(TokenType::EndOfFile)) {
        if (Check(TokenType::Newline) || Check(TokenType::Dedent)) {
            Advance();
            continue;
        }
        if (MatchKeyword("import")) {
            program.imports.push_back(ParseImport());
            continue;
        }
        if (IsStructDeclLine()) {
            program.structs.push_back(ParseStructDecl());
        } else {
            program.functions.push_back(ParseFunctionDecl(false, ""));
        }
    }
    return program;
}

const std::unordered_set<std::string> &Parser::TypeNames() const {
    return type_names_;
}

void Parser::PreScanStructNames() {
    size_t idx = 0;
    while (idx < tokens_.size()) {
        size_t line_start = idx;
        while (idx < tokens_.size() && tokens_[idx].type != TokenType::Newline &&
               tokens_[idx].type != TokenType::EndOfFile) {
            idx++;
        }
        size_t line_end = idx;
        if (line_end > line_start) {
            size_t first = line_start;
            while (first < line_end && (tokens_[first].type == TokenType::Indent || tokens_[first].type == TokenType::Dedent)) {
                first++;
            }
            bool has_paren = false;
            bool has_colon = false;
            for (size_t i = line_start; i < line_end; ++i) {
                if (tokens_[i].type == TokenType::LParen) {
                    has_paren = true;
                    break;
                }
                if (tokens_[i].type == TokenType::Colon) {
                    has_colon = true;
                }
            }
            if (has_colon && !has_paren && first < line_end && tokens_[first].type == TokenType::Identifier) {
                type_names_.insert(tokens_[first].text);
            }
        }
        if (idx < tokens_.size() && tokens_[idx].type == TokenType::Newline) {
            idx++;
        } else if (idx < tokens_.size() && tokens_[idx].type == TokenType::EndOfFile) {
            break;
        }
    }
}

bool Parser::IsStructDeclLine() {
    size_t idx = current_;
    while (idx < tokens_.size() && (tokens_[idx].type == TokenType::Indent || tokens_[idx].type == TokenType::Dedent)) {
        idx++;
    }
    if (idx >= tokens_.size() || tokens_[idx].type != TokenType::Identifier) {
        return false;
    }
    bool has_paren = false;
    bool has_colon = false;
    while (idx < tokens_.size() && tokens_[idx].type != TokenType::Newline &&
           tokens_[idx].type != TokenType::EndOfFile) {
        if (tokens_[idx].type == TokenType::LParen) {
            has_paren = true;
            break;
        }
        if (tokens_[idx].type == TokenType::Colon) {
            has_colon = true;
        }
        idx++;
    }
    return has_colon && !has_paren;
}

ImportDecl Parser::ParseImport() {
    ImportDecl decl;
    if (Check(TokenType::String)) {
        Token path = Advance();
        decl.module = path.text;
        decl.is_path = true;
        decl.line = path.line;
    } else {
        std::string module;
        Token first = Consume(TokenType::Identifier, "Expected module name after import");
        module = first.text;
        while (Match(TokenType::Dot)) {
            Token part = Consume(TokenType::Identifier, "Expected module name after '.'");
            module += ".";
            module += part.text;
        }
        decl.module = module;
        decl.line = first.line;
    }
    if (MatchKeyword("as")) {
        Token alias = Consume(TokenType::Identifier, "Expected alias name after 'as'");
        decl.alias = alias.text;
    }
    Consume(TokenType::Newline, "Expected newline after import");
    return decl;
}

StructDef Parser::ParseStructDecl() {
    StructDef def;
    Token name = Consume(TokenType::Identifier, "Expected struct name");
    def.name = name.text;
    def.line = name.line;
    if (MatchKeyword("extends")) {
        Token parent = Consume(TokenType::Identifier, "Expected parent name");
        def.parent = parent.text;
    }
    Consume(TokenType::Colon, "Expected ':' after struct name");
    Consume(TokenType::Newline, "Expected newline after struct declaration");
    Consume(TokenType::Indent, "Expected indent for struct body");
    while (!Check(TokenType::Dedent) && !Check(TokenType::EndOfFile)) {
        if (Check(TokenType::Newline)) {
            Advance();
            continue;
        }
        if (IsStructDeclLine()) {
            throw CompileError("Nested struct declarations are not allowed at line " + std::to_string(Peek().line));
        }
        if (IsFunctionDeclLine()) {
            def.methods.push_back(ParseFunctionDecl(true, def.name));
        } else {
            def.fields.push_back(ParseStructField());
        }
    }
    Consume(TokenType::Dedent, "Expected end of struct block");
    return def;
}

bool Parser::IsFunctionDeclLine() {
    size_t idx = current_;
    while (idx < tokens_.size() && (tokens_[idx].type == TokenType::Indent || tokens_[idx].type == TokenType::Dedent)) {
        idx++;
    }
    bool has_paren = false;
    bool has_colon = false;
    while (idx < tokens_.size() && tokens_[idx].type != TokenType::Newline &&
           tokens_[idx].type != TokenType::EndOfFile) {
        if (tokens_[idx].type == TokenType::LParen) {
            has_paren = true;
        }
        if (tokens_[idx].type == TokenType::Colon) {
            has_colon = true;
        }
        idx++;
    }
    return has_paren && !has_colon;
}

Function Parser::ParseFunctionDecl(bool is_method, const std::string &owner) {
    Function fn;
    fn.is_method = is_method;
    fn.owner = owner;
    fn.return_type = ParseReturnType();
    Token name = Consume(TokenType::Identifier, "Expected function name");
    fn.name = name.text;
    fn.line = name.line;
    Consume(TokenType::LParen, "Expected '(' after function name");
    if (!Check(TokenType::RParen)) {
        do {
            TypeSpec type = ParseType();
            Token param_name = Consume(TokenType::Identifier, "Expected parameter name");
            fn.params.push_back({type, param_name.text});
        } while (Match(TokenType::Comma));
    }
    Consume(TokenType::RParen, "Expected ')' after parameters");
    Consume(TokenType::Newline, "Expected newline after function signature");
    fn.body = ParseBlock();
    return fn;
}

std::pair<TypeSpec, std::string> Parser::ParseStructField() {
    TypeSpec type = ParseType();
    Token name = Consume(TokenType::Identifier, "Expected field name");
    Consume(TokenType::Newline, "Expected newline after field declaration");
    return {type, name.text};
}

std::vector<StmtPtr> Parser::ParseBlock() {
    Consume(TokenType::Indent, "Expected indent to start block");
    std::vector<StmtPtr> stmts;
    while (!Check(TokenType::Dedent) && !Check(TokenType::EndOfFile)) {
        if (Check(TokenType::Newline)) {
            Advance();
            continue;
        }
        stmts.push_back(ParseStatement());
    }
    Consume(TokenType::Dedent, "Expected end of block");
    return stmts;
}

StmtPtr Parser::ParseStatement() {
    if (MatchKeyword("if")) {
        return ParseIf();
    }
    if (MatchKeyword("while")) {
        return ParseWhile();
    }
    if (MatchKeyword("return")) {
        auto stmt = std::make_shared<Stmt>();
        stmt->kind = StmtKind::Return;
        stmt->line = Previous().line;
        if (!Check(TokenType::Newline)) {
            stmt->expr = ParseExpression();
        }
        Consume(TokenType::Newline, "Expected newline after return");
        return stmt;
    }

    if (IsTypeStart()) {
        auto stmt = std::make_shared<Stmt>();
        stmt->kind = StmtKind::VarDecl;
        stmt->var_type = ParseType();
        Token name = Consume(TokenType::Identifier, "Expected variable name");
        stmt->var_name = name.text;
        if (Match(TokenType::Assign)) {
            stmt->expr = ParseExpression();
        }
        Consume(TokenType::Newline, "Expected newline after declaration");
        stmt->line = name.line;
        return stmt;
    }

    ExprPtr expr = ParseExpression();
    if (Match(TokenType::Assign)) {
        auto stmt = std::make_shared<Stmt>();
        stmt->kind = StmtKind::Assign;
        stmt->target = expr;
        stmt->expr = ParseExpression();
        Consume(TokenType::Newline, "Expected newline after assignment");
        stmt->line = expr->line;
        return stmt;
    }

    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::ExprStmt;
    stmt->expr = expr;
    Consume(TokenType::Newline, "Expected newline after expression");
    stmt->line = expr->line;
    return stmt;
}

StmtPtr Parser::ParseIf() {
    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::If;
    stmt->expr = ParseExpression();
    Consume(TokenType::Newline, "Expected newline after if condition");
    stmt->then_body = ParseBlock();
    if (MatchKeyword("else")) {
        Consume(TokenType::Newline, "Expected newline after else");
        stmt->else_body = ParseBlock();
    }
    stmt->line = stmt->expr->line;
    return stmt;
}

StmtPtr Parser::ParseWhile() {
    auto stmt = std::make_shared<Stmt>();
    stmt->kind = StmtKind::While;
    stmt->expr = ParseExpression();
    Consume(TokenType::Newline, "Expected newline after while condition");
    stmt->body = ParseBlock();
    stmt->line = stmt->expr->line;
    return stmt;
}

TypeSpec Parser::ParseReturnType() {
    if (MatchKeyword("void")) {
        return TypeSpec{"", 0, true};
    }
    return ParseType();
}

TypeSpec Parser::ParseType() {
    TypeSpec type;
    if (Check(TokenType::Keyword)) {
        std::string kw = Peek().text;
        if (kw == "int" || kw == "real" || kw == "bool" || kw == "string") {
            Advance();
            type.name = kw;
        } else {
            throw CompileError("Expected type keyword at line " + std::to_string(Peek().line));
        }
    } else if (Check(TokenType::Identifier)) {
        type.name = Advance().text;
    } else {
        throw CompileError("Expected type name at line " + std::to_string(Peek().line));
    }
    while (Check(TokenType::LBracket) && PeekNext().type == TokenType::RBracket) {
        Advance();
        Consume(TokenType::RBracket, "Expected ']' in array type");
        type.array_depth++;
    }
    return type;
}

ExprPtr Parser::ParseExpression() { return ParseLogicalOr(); }

ExprPtr Parser::ParseLogicalOr() {
    ExprPtr expr = ParseLogicalAnd();
    while (MatchKeyword("or")) {
        expr = MakeBinary("or", expr, ParseLogicalAnd());
    }
    return expr;
}

ExprPtr Parser::ParseLogicalAnd() {
    ExprPtr expr = ParseEquality();
    while (MatchKeyword("and")) {
        expr = MakeBinary("and", expr, ParseEquality());
    }
    return expr;
}

ExprPtr Parser::ParseEquality() {
    ExprPtr expr = ParseRelational();
    while (Match(TokenType::Eq) || Match(TokenType::Neq)) {
        Token op = Previous();
        expr = MakeBinary(op.text, expr, ParseRelational());
    }
    return expr;
}

ExprPtr Parser::ParseRelational() {
    ExprPtr expr = ParseAdditive();
    while (Match(TokenType::Lt) || Match(TokenType::Lte) || Match(TokenType::Gt) || Match(TokenType::Gte)) {
        Token op = Previous();
        expr = MakeBinary(op.text, expr, ParseAdditive());
    }
    return expr;
}

ExprPtr Parser::ParseAdditive() {
    ExprPtr expr = ParseMultiplicative();
    while (Match(TokenType::Plus) || Match(TokenType::Minus)) {
        Token op = Previous();
        expr = MakeBinary(op.text, expr, ParseMultiplicative());
    }
    return expr;
}

ExprPtr Parser::ParseMultiplicative() {
    ExprPtr expr = ParseUnary();
    while (Match(TokenType::Star) || Match(TokenType::Slash) || Match(TokenType::Percent)) {
        Token op = Previous();
        expr = MakeBinary(op.text, expr, ParseUnary());
    }
    return expr;
}

ExprPtr Parser::ParseUnary() {
    if (Match(TokenType::Minus)) {
        return MakeUnary("-", ParseUnary());
    }
    if (Match(TokenType::Bang)) {
        return MakeUnary("!", ParseUnary());
    }
    return ParsePostfix();
}

ExprPtr Parser::ParsePostfix() {
    ExprPtr expr = ParseTerm();
    bool keep = true;
    while (keep) {
        if (Match(TokenType::LBracket)) {
            ExprPtr index = ParseExpression();
            Consume(TokenType::RBracket, "Expected ']' after index");
            auto node = std::make_shared<Expr>();
            node->kind = ExprKind::Index;
            node->base = expr;
            node->left = index;
            node->line = expr->line;
            expr = node;
        } else if (Match(TokenType::Dot)) {
            Token field = Consume(TokenType::Identifier, "Expected field name");
            auto node = std::make_shared<Expr>();
            node->kind = ExprKind::Field;
            node->base = expr;
            node->field = field.text;
            node->line = field.line;
            expr = node;
        } else if (Match(TokenType::LParen)) {
            auto node = std::make_shared<Expr>();
            node->kind = ExprKind::Call;
            node->base = expr;
            if (!Check(TokenType::RParen)) {
                do {
                    node->args.push_back(ParseExpression());
                } while (Match(TokenType::Comma));
            }
            Consume(TokenType::RParen, "Expected ')' after arguments");
            node->line = expr->line;
            expr = node;
        } else {
            keep = false;
        }
    }
    return expr;
}

ExprPtr Parser::ParseTerm() {
    if (Match(TokenType::Integer)) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::IntLit;
        node->int_value = std::stoll(Previous().text);
        node->line = Previous().line;
        return node;
    }
    if (Match(TokenType::Real)) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::RealLit;
        node->real_value = std::stod(Previous().text);
        node->line = Previous().line;
        return node;
    }
    if (Match(TokenType::String)) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::StringLit;
        node->text = Previous().text;
        node->line = Previous().line;
        return node;
    }
    if (MatchKeyword("true") || MatchKeyword("false")) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::BoolLit;
        node->bool_value = Previous().text == "true";
        node->line = Previous().line;
        return node;
    }
    if (MatchKeyword("new")) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::NewExpr;
        node->new_type = ParseType();
        if (Match(TokenType::LBracket)) {
            node->new_size = ParseExpression();
            Consume(TokenType::RBracket, "Expected ']' after new size");
        }
        node->line = Previous().line;
        return node;
    }
    if (Match(TokenType::Identifier)) {
        auto node = std::make_shared<Expr>();
        node->kind = ExprKind::Var;
        node->text = Previous().text;
        node->line = Previous().line;
        return node;
    }
    if (Match(TokenType::LParen)) {
        ExprPtr expr = ParseExpression();
        Consume(TokenType::RParen, "Expected ')' after expression");
        return expr;
    }
    throw CompileError("Unexpected token at line " + std::to_string(Peek().line));
}

ExprPtr Parser::MakeBinary(const std::string &op, ExprPtr left, ExprPtr right) {
    auto node = std::make_shared<Expr>();
    node->kind = ExprKind::Binary;
    node->op = op;
    node->left = left;
    node->right = right;
    node->line = left->line;
    return node;
}

ExprPtr Parser::MakeUnary(const std::string &op, ExprPtr operand) {
    auto node = std::make_shared<Expr>();
    node->kind = ExprKind::Unary;
    node->op = op;
    node->left = operand;
    node->line = operand->line;
    return node;
}

bool Parser::IsTypeStart() {
    if (Check(TokenType::Keyword)) {
        std::string kw = Peek().text;
        return kw == "int" || kw == "real" || kw == "bool" || kw == "string";
    }
    if (Check(TokenType::Identifier)) {
        return type_names_.count(Peek().text) > 0;
    }
    return false;
}

bool Parser::Match(TokenType type) {
    if (Check(type)) {
        Advance();
        return true;
    }
    return false;
}

bool Parser::MatchKeyword(const std::string &kw) {
    if (Check(TokenType::Keyword) && Peek().text == kw) {
        Advance();
        return true;
    }
    return false;
}

Token Parser::Consume(TokenType type, const std::string &msg) {
    if (Check(type)) {
        return Advance();
    }
    throw CompileError(msg + " at line " + std::to_string(Peek().line));
}

bool Parser::Check(TokenType type) const { return Peek().type == type; }

Token Parser::Advance() {
    if (!IsAtEnd()) {
        current_++;
    }
    return Previous();
}

bool Parser::IsAtEnd() const { return Peek().type == TokenType::EndOfFile; }

Token Parser::Peek() const { return tokens_[current_]; }
Token Parser::Previous() const { return tokens_[current_ - 1]; }
Token Parser::PeekNext() const {
    if (current_ + 1 >= tokens_.size()) {
        return tokens_.back();
    }
    return tokens_[current_ + 1];
}
