// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_stmt.h"

#include "common.h"

StmtEmitter::StmtEmitter(std::ostream &out,
                         ExprEmitter emit_expr,
                         AssignmentEmitter emit_assignment,
                         ConditionEmitter emit_condition,
                         ZeroEmitter emit_zero,
                         TypeChecker require_same_type)
    : out_(out),
      emit_expr_(std::move(emit_expr)),
      emit_assignment_(std::move(emit_assignment)),
      emit_condition_(std::move(emit_condition)),
      emit_zero_(std::move(emit_zero)),
      require_same_type_(std::move(require_same_type)) {}

void StmtEmitter::EmitStmt(const StmtPtr &stmt, Env &env) {
    switch (stmt->kind) {
        case StmtKind::VarDecl: {
            auto it = env.locals.find(stmt->var_name);
            if (it == env.locals.end()) {
                throw CompileError("Unknown local variable " + stmt->var_name);
            }
            if (stmt->expr) {
                auto expr_type = emit_expr_(stmt->expr, env);
                require_same_type_(it->second.type, expr_type, stmt->line);
                out_ << "    local.set " << it->second.wasm_name << "\n";
            } else {
                emit_zero_(it->second.type);
                out_ << "    local.set " << it->second.wasm_name << "\n";
            }
            break;
        }
        case StmtKind::Assign: {
            emit_assignment_(stmt->target, stmt->expr, env);
            break;
        }
        case StmtKind::ExprStmt: {
            auto expr_type = emit_expr_(stmt->expr, env);
            if (expr_type->kind != TypeKind::Void) {
                out_ << "    drop\n";
            }
            break;
        }
        case StmtKind::Return: {
            if (stmt->expr) {
                emit_expr_(stmt->expr, env);
                out_ << "    return\n";
            } else {
                out_ << "    return\n";
            }
            break;
        }
        case StmtKind::If: {
            emit_condition_(stmt->expr, env);
            out_ << "    if\n";
            for (const auto &s : stmt->then_body) {
                EmitStmt(s, env);
            }
            if (!stmt->else_body.empty()) {
                out_ << "    else\n";
                for (const auto &s : stmt->else_body) {
                    EmitStmt(s, env);
                }
            }
            out_ << "    end\n";
            break;
        }
        case StmtKind::While: {
            out_ << "    block\n";
            out_ << "      loop\n";
            emit_condition_(stmt->expr, env);
            out_ << "      i32.eqz\n";
            out_ << "      br_if 1\n";
            for (const auto &s : stmt->body) {
                EmitStmt(s, env);
            }
            out_ << "      br 0\n";
            out_ << "      end\n";
            out_ << "    end\n";
            break;
        }
    }
}
