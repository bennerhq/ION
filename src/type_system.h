// THE BEER LICENSE (with extra fizz)
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.h"
#include "codegen_types.h"

// Consolidates TypeResolver, TypeLayout, TypeRules, and TypeChecker

struct TypeContext {
  const std::unordered_map<std::string, StructInfo> &structs;
  std::function<const FunctionInfo *(const std::string &)> lookup_func;
};

// Type Resolution & Layout
void InitStructs(const Program &program,
                 std::unordered_map<std::string, StructInfo> &structs);
std::shared_ptr<Type>
ResolveType(const TypeSpec &spec,
            const std::unordered_map<std::string, StructInfo> &structs);
int64_t GetTypeSize(const std::shared_ptr<Type> &type);
int64_t Align8(int64_t value);

// Type Rules
bool IsAssignable(const std::shared_ptr<Type> &expected,
                  const std::shared_ptr<Type> &actual,
                  const std::unordered_map<std::string, StructInfo> &structs);
void RequireSameType(
    const std::shared_ptr<Type> &expected, const std::shared_ptr<Type> &actual,
    int line, const std::unordered_map<std::string, StructInfo> &structs);

// Type Checking
std::shared_ptr<Type> CheckExpr(const ExprPtr &expr, Env &env,
                                const TypeContext &ctx);
void CheckStmt(const StmtPtr &stmt, Env &env, const TypeContext &ctx);
void CheckStmts(const std::vector<StmtPtr> &stmts, Env &env,
                const TypeContext &ctx);
