// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#pragma once
#include <functional>
#include <memory>
#include <ostream>

#include "codegen_types.h"

class StmtEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using AssignmentEmitter = std::function<void(const ExprPtr &, const ExprPtr &, Env &)>;
    using ConditionEmitter = std::function<void(const ExprPtr &, Env &)>;
    using ZeroEmitter = std::function<void(const std::shared_ptr<Type> &)>;
    using TypeChecker = std::function<void(const std::shared_ptr<Type> &,
                                           const std::shared_ptr<Type> &,
                                           int)>;

    StmtEmitter(std::ostream &out,
                ExprEmitter emit_expr,
                AssignmentEmitter emit_assignment,
                ConditionEmitter emit_condition,
                ZeroEmitter emit_zero,
                TypeChecker require_same_type);

    void EmitStmt(const StmtPtr &stmt, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
    AssignmentEmitter emit_assignment_;
    ConditionEmitter emit_condition_;
    ZeroEmitter emit_zero_;
    TypeChecker require_same_type_;
};
