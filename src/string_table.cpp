// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#include "string_table.h"

#include <algorithm>
#include <cctype>

#include "common.h"
#include "type_system.h"

StringLiteralTable::StringLiteralTable(int64_t base_cursor)
    : base_cursor_(base_cursor), data_cursor_(base_cursor),
      heap_start_(base_cursor) {}

void StringLiteralTable::Build(const Program &program) {
  Reset();
  AddBuiltins();
  for (const auto &fn : program.functions) {
    for (const auto &stmt : fn.body) {
      CollectStrings(stmt);
    }
  }
  for (const auto &def : program.structs) {
    for (const auto &method : def.methods) {
      for (const auto &stmt : method.body) {
        CollectStrings(stmt);
      }
    }
  }
  heap_start_ = Align8(data_cursor_);
}

const std::unordered_map<std::string, int64_t> &
StringLiteralTable::Offsets() const {
  return string_offsets_;
}

const std::vector<std::pair<int64_t, std::string>> &
StringLiteralTable::Segments() const {
  return data_segments_;
}

int64_t StringLiteralTable::HeapStart() const { return heap_start_; }

void StringLiteralTable::Reset() {
  data_cursor_ = base_cursor_;
  heap_start_ = base_cursor_;
  string_offsets_.clear();
  data_segments_.clear();
}

void StringLiteralTable::AddBuiltins() {
  AddStringLiteral("");
  AddStringLiteral("\n");
  AddStringLiteral(".");
  AddStringLiteral("-");
  AddStringLiteral("+");
  AddStringLiteral("e");
  AddStringLiteral("0");
  AddStringLiteral("true");
  AddStringLiteral("false");
}

void StringLiteralTable::CollectStrings(const StmtPtr &stmt) {
  if (!stmt) {
    return;
  }
  switch (stmt->kind) {
  case StmtKind::VarDecl:
  case StmtKind::Assign:
  case StmtKind::ExprStmt:
  case StmtKind::Return:
    CollectStrings(stmt->expr);
    CollectStrings(stmt->target);
    break;
  case StmtKind::If:
    CollectStrings(stmt->expr);
    for (const auto &s : stmt->then_body) {
      CollectStrings(s);
    }
    for (const auto &s : stmt->else_body) {
      CollectStrings(s);
    }
    break;
  case StmtKind::While:
    CollectStrings(stmt->expr);
    for (const auto &s : stmt->body) {
      CollectStrings(s);
    }
    break;
  }
}

void StringLiteralTable::CollectStrings(const ExprPtr &expr) {
  if (!expr) {
    return;
  }
  if (expr->kind == ExprKind::Call && expr->base &&
      expr->base->kind == ExprKind::Var && expr->base->text == "print" &&
      !expr->args.empty() && expr->args[0]->kind == ExprKind::StringLit) {
    if (expr->args.size() > 1 || NeedsFormat(expr->args[0]->text)) {
      AddFormatLiterals(expr->args[0]->text);
    }
  }
  if (expr->kind == ExprKind::StringLit) {
    AddStringLiteral(expr->text);
  }
  if (expr->left) {
    CollectStrings(expr->left);
  }
  if (expr->right) {
    CollectStrings(expr->right);
  }
  if (expr->base) {
    CollectStrings(expr->base);
  }
  if (expr->new_size) {
    CollectStrings(expr->new_size);
  }
  for (const auto &arg : expr->args) {
    CollectStrings(arg);
  }
}

int64_t StringLiteralTable::AddStringLiteral(const std::string &value) {
  auto it = string_offsets_.find(value);
  if (it != string_offsets_.end()) {
    return it->second;
  }
  int64_t offset = Align8(data_cursor_);
  int64_t length = static_cast<int64_t>(value.size());
  std::string bytes;
  bytes.resize(8 + value.size());
  for (int i = 0; i < 8; ++i) {
    bytes[i] = static_cast<char>((length >> (i * 8)) & 0xFF);
  }
  std::copy(value.begin(), value.end(), bytes.begin() + 8);
  data_segments_.push_back({offset, bytes});
  string_offsets_[value] = offset;
  data_cursor_ = offset + static_cast<int64_t>(bytes.size());
  return offset;
}

void StringLiteralTable::AddFormatLiterals(const std::string &format) {
  std::string literal;
  for (size_t i = 0; i < format.size(); ++i) {
    char c = format[i];
    if (c == '%' && i + 1 < format.size()) {
      char next = format[i + 1];
      if (next == '%') {
        literal.push_back('%');
        i++;
        continue;
      }
      if (!literal.empty()) {
        AddStringLiteral(literal);
        literal.clear();
      }
      if (next == 'i' || next == 'b' || next == 's') {
        i++;
        continue;
      }
      if (next == 'r' || next == 'e') {
        i++;
        if (i + 1 < format.size() && format[i + 1] == '{') {
          i += 2;
          if (i >= format.size() ||
              !std::isdigit(static_cast<unsigned char>(format[i]))) {
            throw CompileError("Format precision requires digits");
          }
          while (i < format.size() &&
                 std::isdigit(static_cast<unsigned char>(format[i]))) {
            i++;
          }
          if (i >= format.size() || format[i] != '}') {
            throw CompileError("Format precision missing '}'");
          }
        }
        continue;
      }
      throw CompileError("Unsupported format specifier in print");
    }
    literal.push_back(c);
  }
  if (!literal.empty()) {
    AddStringLiteral(literal);
  }
}

bool StringLiteralTable::NeedsFormat(const std::string &format) {
  for (char c : format) {
    if (c == '%' || c == '\n') {
      return true;
    }
  }
  return false;
}

int64_t StringLiteralTable::Align8(int64_t value) { return ::Align8(value); }
