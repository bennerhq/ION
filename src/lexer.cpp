// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#include <cctype>
#include <sstream>
#include <unordered_set>

#include "lexer.h"

Lexer::Lexer(const std::string &source) : source_(source) {}

std::vector<Token> Lexer::Tokenize() {
    std::vector<Token> tokens;
    std::vector<int> indent_stack;
    indent_stack.push_back(0);
    int line_no = 1;
    std::istringstream input(source_);
    std::string line;
    while (std::getline(input, line)) {
        std::string trimmed = StripComment(line);
        if (trimmed.empty()) {
            line_no++;
            continue;
        }
        int indent = CountIndent(trimmed);
        if (indent % 4 != 0) {
            throw CompileError("Indentation must be a multiple of 4 spaces at line " + std::to_string(line_no));
        }
        int level = indent / 4;
        if (level > indent_stack.back()) {
            while (level > indent_stack.back()) {
                indent_stack.push_back(indent_stack.back() + 1);
                tokens.push_back({TokenType::Indent, "", line_no});
            }
        } else if (level < indent_stack.back()) {
            while (level < indent_stack.back()) {
                indent_stack.pop_back();
                tokens.push_back({TokenType::Dedent, "", line_no});
            }
        }

        LexLine(trimmed, line_no, tokens);
        tokens.push_back({TokenType::Newline, "", line_no});
        line_no++;
    }
    while (indent_stack.back() > 0) {
        indent_stack.pop_back();
        tokens.push_back({TokenType::Dedent, "", line_no});
    }
    tokens.push_back({TokenType::EndOfFile, "", line_no});
    return tokens;
}

std::string Lexer::StripComment(const std::string &line) {
    bool in_string = false;
    std::string out;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"' && (i == 0 || line[i - 1] != '\\')) {
            in_string = !in_string;
            out.push_back(c);
            continue;
        }
        if (!in_string && c == '#') {
            break;
        }
        out.push_back(c);
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
        out.pop_back();
    }
    bool only_ws = true;
    for (char c : out) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            only_ws = false;
            break;
        }
    }
    if (only_ws) {
        return "";
    }
    return out;
}

int Lexer::CountIndent(const std::string &line) {
    int count = 0;
    for (char c : line) {
        if (c == ' ') {
            count++;
        } else if (c == '\t') {
            count += 4;
        } else {
            break;
        }
    }
    return count;
}

void Lexer::LexLine(const std::string &line, int line_no, std::vector<Token> &tokens) {
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
        i++;
    }
    while (i < line.size()) {
        char c = line[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            i++;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                i++;
            }
            std::string word = line.substr(start, i - start);
            if (IsKeyword(word)) {
                tokens.push_back({TokenType::Keyword, word, line_no});
            } else {
                tokens.push_back({TokenType::Identifier, word, line_no});
            }
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i;
            bool is_real = false;
            while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
                i++;
            }
            if (i < line.size() && line[i] == '.') {
                is_real = true;
                i++;
                while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
                    i++;
                }
            }
            std::string num = line.substr(start, i - start);
            tokens.push_back({is_real ? TokenType::Real : TokenType::Integer, num, line_no});
            continue;
        }
        if (c == '"') {
            i++;
            std::string str;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    char esc = line[i + 1];
                    if (esc == 'n') {
                        str.push_back('\n');
                    } else if (esc == 't') {
                        str.push_back('\t');
                    } else if (esc == '"') {
                        str.push_back('"');
                    } else if (esc == '\\') {
                        str.push_back('\\');
                    } else {
                        str.push_back(esc);
                    }
                    i += 2;
                } else {
                    str.push_back(line[i]);
                    i++;
                }
            }
            if (i >= line.size() || line[i] != '"') {
                throw CompileError("Unterminated string literal at line " + std::to_string(line_no));
            }
            i++;
            tokens.push_back({TokenType::String, str, line_no});
            continue;
        }
        switch (c) {
            case '(':
                tokens.push_back({TokenType::LParen, "(", line_no});
                i++;
                break;
            case ')':
                tokens.push_back({TokenType::RParen, ")", line_no});
                i++;
                break;
            case '[':
                tokens.push_back({TokenType::LBracket, "[", line_no});
                i++;
                break;
            case ']':
                tokens.push_back({TokenType::RBracket, "]", line_no});
                i++;
                break;
            case ',':
                tokens.push_back({TokenType::Comma, ",", line_no});
                i++;
                break;
            case '.':
                tokens.push_back({TokenType::Dot, ".", line_no});
                i++;
                break;
            case ':':
                tokens.push_back({TokenType::Colon, ":", line_no});
                i++;
                break;
            case '+':
                tokens.push_back({TokenType::Plus, "+", line_no});
                i++;
                break;
            case '-':
                tokens.push_back({TokenType::Minus, "-", line_no});
                i++;
                break;
            case '*':
                tokens.push_back({TokenType::Star, "*", line_no});
                i++;
                break;
            case '/':
                tokens.push_back({TokenType::Slash, "/", line_no});
                i++;
                break;
            case '%':
                tokens.push_back({TokenType::Percent, "%", line_no});
                i++;
                break;
            case '=':
                if (i + 1 < line.size() && line[i + 1] == '=') {
                    tokens.push_back({TokenType::Eq, "==", line_no});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Assign, "=", line_no});
                    i++;
                }
                break;
            case '!':
                if (i + 1 < line.size() && line[i + 1] == '=') {
                    tokens.push_back({TokenType::Neq, "!=", line_no});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Bang, "!", line_no});
                    i++;
                }
                break;
            case '<':
                if (i + 1 < line.size() && line[i + 1] == '=') {
                    tokens.push_back({TokenType::Lte, "<=", line_no});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Lt, "<", line_no});
                    i++;
                }
                break;
            case '>':
                if (i + 1 < line.size() && line[i + 1] == '=') {
                    tokens.push_back({TokenType::Gte, ">=", line_no});
                    i += 2;
                } else {
                    tokens.push_back({TokenType::Gt, ">", line_no});
                    i++;
                }
                break;
            default:
                throw CompileError(std::string("Unexpected character '") + c + "' at line " + std::to_string(line_no));
        }
    }
}

bool Lexer::IsKeyword(const std::string &word) {
    static const std::unordered_set<std::string> keywords = {
        "int", "real", "bool", "string", "void", "if", "else", "while", "return",
        "true", "false", "new", "and", "or", "extends", "import", "as"
    };
    return keywords.count(word) > 0;
}
