// THE BEER LICENSE (with extra fizz)
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include "ast.h"
#include "codegen_types.h"

using FunctionLookup = std::function<const FunctionInfo *(const std::string &)>;
using StructLookup = std::function<const StructInfo *(const std::string &)>;
using TypeResolverFn = std::function<std::shared_ptr<Type>(const TypeSpec &)>;

class TypeChecker {
public:
  TypeChecker(TypeResolverFn resolve_type, FunctionLookup lookup_function,
              StructLookup lookup_struct);

  std::shared_ptr<Type> Check(const ExprPtr &expr, Env &env);
  void Check(const StmtPtr &stmt, Env &env);
  void Check(const std::vector<StmtPtr> &stmts, Env &env);

private:
  TypeResolverFn resolve_type_;
  FunctionLookup lookup_function_;
  StructLookup lookup_struct_;

  std::shared_ptr<Type> CheckVar(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckUnary(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckBinary(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckField(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckIndex(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckCall(const ExprPtr &expr, Env &env);
  std::shared_ptr<Type> CheckNew(const ExprPtr &expr, Env &env);
};
