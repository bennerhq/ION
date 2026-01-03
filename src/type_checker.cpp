// THE BEER LICENSE (with extra fizz)
#include "type_checker.h"

#include "common.h"
#include "identifier_lookup.h"

TypeChecker::TypeChecker(TypeResolverFn resolve_type,
                         FunctionLookup lookup_function,
                         StructLookup lookup_struct)
    : resolve_type_(std::move(resolve_type)),
      lookup_function_(std::move(lookup_function)),
      lookup_struct_(std::move(lookup_struct)) {}

void TypeChecker::Check(const std::vector<StmtPtr> &stmts, Env &env) {
  for (const auto &stmt : stmts) {
    Check(stmt, env);
  }
}

void TypeChecker::Check(const StmtPtr &stmt, Env &env) {
  if (!stmt)
    return;
  switch (stmt->kind) {
  case StmtKind::VarDecl:
    if (stmt->expr) {
      Check(stmt->expr, env);
      auto var_type = resolve_type_(stmt->var_type);
      env.locals[stmt->var_name] = LocalInfo{stmt->var_name, var_type};
    } else {
      auto var_type = resolve_type_(stmt->var_type);
      env.locals[stmt->var_name] = LocalInfo{stmt->var_name, var_type};
    }
    break;
  case StmtKind::Assign: {
    Check(stmt->target, env);
    Check(stmt->expr, env);
    break;
  }
  case StmtKind::If:
    Check(stmt->expr, env);
    {
      Env then_env = env;
      Check(stmt->then_body, then_env);
      if (!stmt->else_body.empty()) {
        Env else_env = env;
        Check(stmt->else_body, else_env);
      }
    }
    break;
  case StmtKind::While:
    Check(stmt->expr, env);
    {
      Env loop_env = env;
      Check(stmt->body, loop_env);
    }
    break;
  case StmtKind::Return:
    if (stmt->expr) {
      Check(stmt->expr, env);
    }
    break;
  case StmtKind::ExprStmt:
    Check(stmt->expr, env);
    break;
  }
}

std::shared_ptr<Type> TypeChecker::Check(const ExprPtr &expr, Env &env) {
  if (!expr)
    return nullptr;
  if (expr->type) {
    return expr->type;
  }

  std::shared_ptr<Type> type;
  switch (expr->kind) {
  case ExprKind::IntLit:
    type = resolve_type_(TypeSpec{"int", 0, false});
    break;
  case ExprKind::RealLit:
    type = resolve_type_(TypeSpec{"real", 0, false});
    break;
  case ExprKind::StringLit:
    type = resolve_type_(TypeSpec{"string", 0, false});
    break;
  case ExprKind::BoolLit:
    type = resolve_type_(TypeSpec{"bool", 0, false});
    break;
  case ExprKind::Var:
    type = CheckVar(expr, env);
    break;
  case ExprKind::Unary:
    type = CheckUnary(expr, env);
    break;
  case ExprKind::Binary:
    type = CheckBinary(expr, env);
    break;
  case ExprKind::Field:
    type = CheckField(expr, env);
    break;
  case ExprKind::Index:
    type = CheckIndex(expr, env);
    break;
  case ExprKind::Call:
    type = CheckCall(expr, env);
    break;
  case ExprKind::NewExpr:
    type = CheckNew(expr, env);
    break;
  default:
    throw CompileError("Unhandled expression at line " +
                       std::to_string(expr->line));
  }
  expr->type = type;
  return type;
}

std::shared_ptr<Type> TypeChecker::CheckVar(const ExprPtr &expr, Env &env) {
  auto result = IdentifierLookup::Find(expr->text, env, lookup_struct_);
  if (result) {
    if (result->kind == IdentifierLookup::Kind::Field) {
      return result->field->type;
    }
    return result->local->type;
  }
  throw CompileError("Unknown identifier " + expr->text + " at line " +
                     std::to_string(expr->line));
}

std::shared_ptr<Type> TypeChecker::CheckUnary(const ExprPtr &expr, Env &env) {
  auto operand = Check(expr->left, env);
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

std::shared_ptr<Type> TypeChecker::CheckBinary(const ExprPtr &expr, Env &env) {
  auto left = Check(expr->left, env);
  auto right = Check(expr->right, env);
  if (expr->op == "+" || expr->op == "-" || expr->op == "*" ||
      expr->op == "/" || expr->op == "%") {
    if (left->kind == TypeKind::Int && right->kind == TypeKind::Int)
      return left;
    if (left->kind == TypeKind::Real && right->kind == TypeKind::Real)
      return left;
    if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
        (left->kind == TypeKind::Int && right->kind == TypeKind::Real))
      return resolve_type_(TypeSpec{"real", 0, false});
    throw CompileError("Arithmetic requires int or real at line " +
                       std::to_string(expr->line));
  }
  if (expr->op == "==" || expr->op == "!=" || expr->op == "<" ||
      expr->op == "<=" || expr->op == ">" || expr->op == ">=") {
    if (left->kind == right->kind) {
      if (left->kind == TypeKind::Int || left->kind == TypeKind::Bool ||
          left->kind == TypeKind::Real)
        return resolve_type_(TypeSpec{"bool", 0, false});
    }
    if ((left->kind == TypeKind::Real && right->kind == TypeKind::Int) ||
        (left->kind == TypeKind::Int && right->kind == TypeKind::Real))
      return resolve_type_(TypeSpec{"bool", 0, false});
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

std::shared_ptr<Type> TypeChecker::CheckField(const ExprPtr &expr, Env &env) {
  auto base_type = Check(expr->base, env);
  if (base_type->kind != TypeKind::Struct) {
    throw CompileError("Field access on non-struct at line " +
                       std::to_string(expr->line));
  }
  const StructInfo *info = lookup_struct_(base_type->name);
  if (!info) {
    throw CompileError("Unknown struct " + base_type->name + " at line " +
                       std::to_string(expr->line));
  }
  auto it = info->field_map.find(expr->field);
  if (it == info->field_map.end()) {
    throw CompileError("Unknown field " + expr->field + " on struct " +
                       base_type->name);
  }
  return it->second.type;
}

std::shared_ptr<Type> TypeChecker::CheckIndex(const ExprPtr &expr, Env &env) {
  auto base_type = Check(expr->base, env);
  if (base_type->kind != TypeKind::Array)
    throw CompileError("Not an array at line " + std::to_string(expr->line));
  auto index_type = Check(expr->left, env);
  if (index_type->kind != TypeKind::Int)
    throw CompileError("Index must be int at line " +
                       std::to_string(expr->line));
  return base_type->element;
}

std::shared_ptr<Type> TypeChecker::CheckCall(const ExprPtr &expr, Env &env) {
  for (auto &arg : expr->args)
    Check(arg, env);
  if (expr->base->kind == ExprKind::Var) {
    std::string name = expr->base->text;
    if (name == "print")
      return resolve_type_(TypeSpec{"void", 0, true});
    if (name == "sqrt")
      return resolve_type_(TypeSpec{"real", 0, false});
    const FunctionInfo *info = lookup_function_(name);
    if (!info)
      throw CompileError("Unknown function " + name + " at line " +
                         std::to_string(expr->line));
    return info->return_type;
  }
  if (expr->base->kind == ExprKind::Field) {
    auto field = expr->base;
    auto base_type = Check(field->base, env);
    if ((base_type->kind == TypeKind::Array ||
         base_type->kind == TypeKind::String) &&
        field->field == "length")
      return resolve_type_(TypeSpec{"int", 0, false});
    if (base_type->kind != TypeKind::Struct)
      throw CompileError("Method on non-struct at line " +
                         std::to_string(expr->line));
    std::string method_name = base_type->name + "." + field->field;
    const FunctionInfo *info = lookup_function_(method_name);
    if (!info)
      throw CompileError("Unknown method " + method_name + " at line " +
                         std::to_string(expr->line));
    return info->return_type;
  }
  throw CompileError("Unsupported call");
}

std::shared_ptr<Type> TypeChecker::CheckNew(const ExprPtr &expr, Env &env) {
  if (expr->new_size) {
    auto size_type = Check(expr->new_size, env);
    if (size_type->kind != TypeKind::Int)
      throw CompileError("Array size int needed");
    auto base = resolve_type_(expr->new_type);
    auto type = std::make_shared<Type>();
    type->kind = TypeKind::Array;
    type->element = base;
    return type;
  }
  return resolve_type_(expr->new_type);
}
