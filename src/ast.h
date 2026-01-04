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
#include <memory>
#include <string>
#include <utility>
#include <vector>

enum class TypeKind { Int, Real, Bool, String, Void, Struct, Array };

struct Type {
  TypeKind kind;
  std::string name;
  std::shared_ptr<Type> element;
};

#include "common.h"

enum class TokenType {
  Identifier,
  Integer,
  Real,
  String,
  Newline,
  Indent,
  Dedent,
  LParen,
  RParen,
  LBracket,
  RBracket,
  Comma,
  Dot,
  Colon,
  Plus,
  Minus,
  Star,
  Slash,
  Percent,
  Assign,
  Eq,
  Neq,
  Lt,
  Lte,
  Gt,
  Gte,
  Bang,
  EndOfFile,
  Keyword
};

struct Token {
  TokenType type;
  std::string text;
  int line = 0;
};

struct TypeSpec {
  std::string name;
  int array_depth = 0;
  bool is_void = false;
};

struct Expr;
struct Stmt;
using ExprPtr = std::shared_ptr<Expr>;
using StmtPtr = std::shared_ptr<Stmt>;

enum class ExprKind {
  IntLit,
  RealLit,
  StringLit,
  BoolLit,
  Var,
  Unary,
  Binary,
  Call,
  Field,
  Index,
  NewExpr
};

struct Expr {
  ExprKind kind;
  std::string text;
  int64_t int_value = 0;
  double real_value = 0.0;
  bool bool_value = false;
  std::string op;
  ExprPtr left;
  ExprPtr right;
  ExprPtr base;
  std::string field;
  std::vector<ExprPtr> args;
  TypeSpec new_type;
  ExprPtr new_size;
  std::shared_ptr<Type> type;
  int line = 0;
};

enum class StmtKind { VarDecl, Assign, If, While, Return, ExprStmt };

struct Stmt {
  StmtKind kind;
  TypeSpec var_type;
  std::string var_name;
  ExprPtr expr;
  ExprPtr target;
  std::vector<StmtPtr> then_body;
  std::vector<StmtPtr> else_body;
  std::vector<StmtPtr> body;
  int line = 0;
};

struct Function {
  std::string name;
  TypeSpec return_type;
  std::vector<std::pair<TypeSpec, std::string>> params;
  std::vector<StmtPtr> body;
  bool is_method = false;
  std::string owner;
  int line = 0;
};

struct StructDef {
  std::string name;
  std::string parent;
  std::vector<std::pair<TypeSpec, std::string>> fields;
  std::vector<Function> methods;
  int line = 0;
};

struct ImportDecl {
  std::string module;
  std::string alias;
  bool is_path = false;
  int line = 0;
};

struct Program {
  std::vector<ImportDecl> imports;
  std::vector<StructDef> structs;
  std::vector<Function> functions;
};
