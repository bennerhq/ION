// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#include "type_system.h"
#include "common.h"
#include "semantics.h"

#include <iostream>
#include <optional>

// --- Type Resolution & Layout ---

void InitStructs(const Program &program,
                 std::unordered_map<std::string, StructInfo> &structs) {
  for (const auto &def : program.structs) {
    StructInfo info;
    info.name = def.name;
    info.parent = def.parent;
    structs[def.name] = info;
  }
}

std::shared_ptr<Type>
ResolveType(const TypeSpec &spec,
            const std::unordered_map<std::string, StructInfo> &structs) {
  if (spec.is_void) {
    return std::make_shared<Type>(Type{TypeKind::Void, "void", nullptr});
  }
  std::cerr << "ResolveType: " << spec.name << std::endl;
  std::shared_ptr<Type> base;
  if (spec.name == "int") {
    base = std::make_shared<Type>(Type{TypeKind::Int, "int", nullptr});
  } else if (spec.name == "real") {
    base = std::make_shared<Type>(Type{TypeKind::Real, "real", nullptr});
  } else if (spec.name == "bool") {
    base = std::make_shared<Type>(Type{TypeKind::Bool, "bool", nullptr});
  } else if (spec.name == "string") {
    base = std::make_shared<Type>(Type{TypeKind::String, "string", nullptr});
  } else {
    if (structs.count(spec.name) == 0) {
      throw CompileError("Unknown type '" + spec.name + "'");
    }
    base = std::make_shared<Type>(Type{TypeKind::Struct, spec.name, nullptr});
  }
  for (int i = 0; i < spec.array_depth; ++i) {
    base = std::make_shared<Type>(Type{TypeKind::Array, "", base});
  }
  std::cerr << "ResolveType returning addr: " << base.get() << std::endl;
  return base;
}

int64_t Align8(int64_t value) { return (value + 7) & ~static_cast<int64_t>(7); }

int64_t GetTypeSize(const std::shared_ptr<Type> &type) {
  // Basic types are 64-bit in this implementation
  switch (type->kind) {
  case TypeKind::Int:
  case TypeKind::Real:
  case TypeKind::Bool:
  case TypeKind::String:
  case TypeKind::Struct:
  case TypeKind::Array:
    return 8;
  case TypeKind::Void:
    return 0;
  }
  return 8;
}

// --- Type Rules ---

bool IsAssignable(const std::shared_ptr<Type> &expected,
                  const std::shared_ptr<Type> &actual,
                  const std::unordered_map<std::string, StructInfo> &structs) {
  if (expected->kind != actual->kind) {
    return false;
  }
  if (expected->kind == TypeKind::Array) {
    return IsAssignable(expected->element, actual->element, structs);
  }
  if (expected->kind == TypeKind::Struct) {
    if (expected->name == actual->name) {
      return true;
    }
    std::string current = actual->name;
    while (!current.empty()) {
      auto it = structs.find(current);
      if (it == structs.end()) {
        break;
      }
      if (it->second.parent == expected->name) {
        return true;
      }
      current = it->second.parent;
    }
    return false;
  }
  return true;
}

void RequireSameType(
    const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual,
    int line, const std::unordered_map<std::string, StructInfo> &structs) {
  if (!IsAssignable(expected, actual, structs)) {
    throw CompileError("Type mismatch at line " + std::to_string(line));
  }
}

// --- Type Checking Helper Functions ---

static std::shared_ptr<Type> CheckVar(const ExprPtr &expr, Env &env,
                                      const TypeContext &ctx) {
  auto result = FindIdentifier(expr->text, env, ctx.structs);
  if (result) {
    if (result->kind == LookupResult::Kind::Field) {
      return result->field->type;
    }
    return result->local->type;
  }
  throw CompileError("Unknown identifier " + expr->text + " at line " +
                     std::to_string(expr->line));
}

static std::shared_ptr<Type> CheckUnary(const ExprPtr &expr, Env &env,
                                        const TypeContext &ctx) {
  auto operand = CheckExpr(expr->left, env, ctx);
  if (expr->op == "-") {
    if (operand->kind == TypeKind::Int || operand->kind == TypeKind::Real) {
      return operand;
    }
    throw CompileError("Unary '-' requires int or real at line " +
                       std::to_string(expr->line));
  }
  if (expr->op == "!") {
    if (operand->kind == TypeKind::Bool) {
      return operand;
    }
    throw CompileError("Unary '!' requires bool at line " +
                       std::to_string(expr->line));
  }
  throw CompileError("Invalid unary operator at line " +
                     std::to_string(expr->line));
}

static std::shared_ptr<Type> CheckBinary(const ExprPtr &expr, Env &env,
                                         const TypeContext &ctx) {
  // std::cerr << "CheckBinary: Short circuit return Int" << std::endl;
  // return ResolveType(TypeSpec{"int", 0, false}, ctx.structs);
  auto left = CheckExpr(expr->left, env, ctx);
  auto right = CheckExpr(expr->right, env, ctx);
  std::cerr << "CheckBinary: left kind " << (int)left->kind << ", right kind "
            << (int)right->kind << std::endl;
  if (expr->op == "+" || expr->op == "-" || expr->op == "*" ||
      expr->op == "/" || expr->op == "%") {
    if (left->kind == TypeKind::Int && right->kind == TypeKind::Int) {
      std::cerr << "CheckBinary returning Fresh Int" << std::endl;
      return ResolveType(TypeSpec{"int", 0, false}, ctx.structs);
    }
    if (left->kind == TypeKind::Real && right->kind == TypeKind::Real)
      return left;
    if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
        (left->kind == TypeKind::Int && right->kind == TypeKind::Real))
      return ResolveType(TypeSpec{"real", 0, false}, ctx.structs);
    throw CompileError("Arithmetic requires int or real at line " +
                       std::to_string(expr->line));
  }
  if (expr->op == "==" || expr->op == "!=" || expr->op == "<" ||
      expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
    if (left->kind == right->kind) {
      if (left->kind == TypeKind::Int || left->kind == TypeKind::Bool ||
          left->kind == TypeKind::Real)
        return ResolveType(TypeSpec{"bool", 0, false}, ctx.structs);
    }
    if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
        (left->kind == TypeKind::Int && right->kind == TypeKind::Real))
      return ResolveType(TypeSpec{"bool", 0, false}, ctx.structs);
    throw CompileError("Comparison type parsing failed at line " +
                       std::to_string(expr->line));
  }
  if (expr->op == "and" || expr->op == "or") {
    if (left->kind == TypeKind::Bool && right->kind == TypeKind::Bool)
      return left;
    throw CompileError("Logical operators require bool at line " +
                       std::to_string(expr->line));
  }
  throw CompileError("Unknown binary operator at line " +
                     std::to_string(expr->line));
}

static std::shared_ptr<Type> CheckField(const ExprPtr &expr, Env &env,
                                        const TypeContext &ctx) {
  auto base_type = CheckExpr(expr->base, env, ctx);
  if (base_type->kind != TypeKind::Struct) {
    throw CompileError("Field access on non-struct at line " +
                       std::to_string(expr->line));
  }

  auto it = ctx.structs.find(base_type->name);
  if (it == ctx.structs.end()) {
    throw CompileError("Unknown struct " + base_type->name + " at line " +
                       std::to_string(expr->line));
  }
  const StructInfo &info = it->second;

  auto field_it = info.field_map.find(expr->field);
  if (field_it == info.field_map.end()) {
    throw CompileError("Unknown field " + expr->field + " on struct " +
                       base_type->name);
  }
  return field_it->second.type;
}

static std::shared_ptr<Type> CheckIndex(const ExprPtr &expr, Env &env,
                                        const TypeContext &ctx) {
  auto base_type = CheckExpr(expr->base, env, ctx);
  if (base_type->kind != TypeKind::Array)
    throw CompileError("Not an array at line " + std::to_string(expr->line));
  auto index_type = CheckExpr(expr->left, env, ctx);
  if (index_type->kind != TypeKind::Int)
    throw CompileError("Index must be int at line " +
                       std::to_string(expr->line));
  return base_type->element;
}

static std::shared_ptr<Type> CheckCall(const ExprPtr &expr, Env &env,
                                       const TypeContext &ctx) {
  for (auto &arg : expr->args)
    CheckExpr(arg, env, ctx);

  if (expr->base->kind == ExprKind::Var) {
    std::string name = expr->base->text;
    if (name == "print")
      return ResolveType(TypeSpec{"void", 0, true}, ctx.structs);
    if (name == "sqrt")
      return ResolveType(TypeSpec{"real", 0, false}, ctx.structs);

    const FunctionInfo *info = ctx.lookup_func(name);
    if (!info)
      throw CompileError("Unknown function " + name + " at line " +
                         std::to_string(expr->line));
    return info->return_type;
  }
  if (expr->base->kind == ExprKind::Field) {
    auto field = expr->base;
    auto base_type = CheckExpr(field->base, env, ctx);
    if ((base_type->kind == TypeKind::Array ||
         base_type->kind == TypeKind::String) &&
        field->field == "length")
      return ResolveType(TypeSpec{"int", 0, false}, ctx.structs);
    if (base_type->kind != TypeKind::Struct)
      throw CompileError("Method on non-struct at line " +
                         std::to_string(expr->line));
    std::string method_name = base_type->name + "." + field->field;
    const FunctionInfo *info = ctx.lookup_func(method_name);
    if (!info)
      throw CompileError("Unknown method " + method_name + " at line " +
                         std::to_string(expr->line));
    return info->return_type;
  }
  throw CompileError("Unsupported call");
}

static std::shared_ptr<Type> CheckNew(const ExprPtr &expr, Env &env,
                                      const TypeContext &ctx) {
  if (expr->new_size) {
    auto size_type = CheckExpr(expr->new_size, env, ctx);
    if (size_type->kind != TypeKind::Int)
      throw CompileError("Array size int needed");
    auto base = ResolveType(expr->new_type, ctx.structs);
    auto type = std::make_shared<Type>();
    type->kind = TypeKind::Array;
    type->element = base;
    return type;
  }
  return ResolveType(expr->new_type, ctx.structs);
}

// --- Main Type Checking Functions ---

std::shared_ptr<Type> CheckExpr(const ExprPtr &expr, Env &env,
                                const TypeContext &ctx) {
  if (!expr)
    return nullptr;
  std::cerr << "CheckExpr kind: " << (int)expr->kind << " line: " << expr->line
            << std::endl;
  if (expr->type)
    return expr->type;

  std::shared_ptr<Type> type;
  switch (expr->kind) {
  case ExprKind::IntLit:
    type = ResolveType(TypeSpec{"int", 0, false}, ctx.structs);
    break;
  case ExprKind::RealLit:
    type = ResolveType(TypeSpec{"real", 0, false}, ctx.structs);
    break;
  case ExprKind::StringLit:
    type = ResolveType(TypeSpec{"string", 0, false}, ctx.structs);
    break;
  case ExprKind::BoolLit:
    type = ResolveType(TypeSpec{"bool", 0, false}, ctx.structs);
    break;
  case ExprKind::Var:
    type = CheckVar(expr, env, ctx);
    break;
  case ExprKind::Unary:
    type = CheckUnary(expr, env, ctx);
    break;
  case ExprKind::Binary:
    type = CheckBinary(expr, env, ctx);
    break;
  case ExprKind::Field:
    type = CheckField(expr, env, ctx);
    break;
  case ExprKind::Index:
    type = CheckIndex(expr, env, ctx);
    break;
  case ExprKind::Call:
    type = CheckCall(expr, env, ctx);
    break;
  case ExprKind::NewExpr:
    type = CheckNew(expr, env, ctx);
    break;
  default:
    throw CompileError("Unhandled expression at line " +
                       std::to_string(expr->line));
  }
  expr->type = type;
  std::cerr << "CheckExpr Returning type kind " << (int)type->kind << std::endl;
  return type;
}

void CheckStmt(const StmtPtr &stmt, Env &env, const TypeContext &ctx) {
  if (!stmt)
    return;
  std::cerr << "CheckStmt kind: " << (int)stmt->kind << " line: " << stmt->line
            << std::endl;
  switch (stmt->kind) {
  case StmtKind::VarDecl:
    if (stmt->expr) {
      CheckExpr(stmt->expr, env, ctx);
      auto var_type = ResolveType(stmt->var_type, ctx.structs);
      env.locals[stmt->var_name] = LocalInfo{stmt->var_name, var_type};
    } else {
      auto var_type = ResolveType(stmt->var_type, ctx.structs);
      env.locals[stmt->var_name] = LocalInfo{stmt->var_name, var_type};
    }
    break;
  case StmtKind::Assign:
    std::cerr << "CheckStmt Assign Start at line " << stmt->line << std::endl;
    CheckExpr(stmt->target, env, ctx);
    CheckExpr(stmt->expr, env, ctx);
    std::cerr << "CheckStmt Assign End at line " << stmt->line << std::endl;
    break;
  case StmtKind::If:
    CheckExpr(stmt->expr, env, ctx);
    {
      Env then_env = env;
      CheckStmts(stmt->then_body, then_env, ctx);
      if (!stmt->else_body.empty()) {
        Env else_env = env;
        CheckStmts(stmt->else_body, else_env, ctx);
      }
    }
    break;
  case StmtKind::While:
    CheckExpr(stmt->expr, env, ctx);
    {
      Env loop_env = env;
      CheckStmts(stmt->body, loop_env, ctx);
    }
    break;
  case StmtKind::Return:
    if (stmt->expr) {
      CheckExpr(stmt->expr, env, ctx);
    }
    break;
  case StmtKind::ExprStmt:
    CheckExpr(stmt->expr, env, ctx);
    break;
  }
}

void CheckStmts(const std::vector<StmtPtr> &stmts, Env &env,
                const TypeContext &ctx) {
  for (const auto &stmt : stmts) {
    CheckStmt(stmt, env, ctx);
  }
}
