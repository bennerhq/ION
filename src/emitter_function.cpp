// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_function.h"

FunctionEmitter::FunctionEmitter(std::ostream &out,
                                 WasmTypeEmitter wasm_type,
                                 TypeResolver resolve_type,
                                 StmtEmitter emit_stmt,
                                 ZeroEmitter emit_zero)
    : out_(out),
      wasm_type_(std::move(wasm_type)),
      resolve_type_(std::move(resolve_type)),
      emit_stmt_(std::move(emit_stmt)),
      emit_zero_(std::move(emit_zero)) {}

void FunctionEmitter::EmitFunction(const Function &fn, const std::string &owner, const FunctionInfo &info) {
    out_ << "  (func " << info.wasm_name;
    if (owner.empty()) {
        out_ << " (export \"" << fn.name << "\")";
    }
    Env env;
    env.current_struct = owner;
    for (size_t i = 0; i < info.params.size(); ++i) {
        std::string param_name;
        std::string wasm_type = wasm_type_(info.params[i]);
        if (owner.empty() || i > 0) {
            param_name = "$p" + std::to_string(i);
        } else {
            param_name = "$this";
        }
        out_ << " (param " << param_name << " " << wasm_type << ")";
        LocalInfo local{param_name, info.params[i]};
        if (owner.empty() || i > 0) {
            env.params[fn.params[i - (owner.empty() ? 0 : 1)].second] = local;
        } else {
            env.params["this"] = local;
        }
    }
    if (info.return_type->kind != Type::Kind::Void) {
        out_ << " (result " << wasm_type_(info.return_type) << ")";
    }
    std::vector<LocalInfo> locals;
    for (const auto &stmt : fn.body) {
        CollectLocals(stmt, env, locals);
    }
    out_ << " (local $tmp0 i64) (local $tmp1 i64) (local $tmp2 i64) (local $tmpf f64)";
    for (const auto &local : locals) {
        out_ << " (local " << local.wasm_name << " " << wasm_type_(local.type) << ")";
    }
    out_ << "\n";
    for (const auto &stmt : fn.body) {
        emit_stmt_(stmt, env);
    }
    if (info.return_type->kind == Type::Kind::Void) {
        out_ << "    (nop)\n";
    } else {
        emit_zero_(info.return_type);
        out_ << "    return\n";
    }
    out_ << "  )\n";
}

void FunctionEmitter::CollectLocals(const StmtPtr &stmt, Env &env, std::vector<LocalInfo> &locals) {
    if (!stmt) {
        return;
    }
    if (stmt->kind == StmtKind::VarDecl) {
        LocalInfo local;
        local.type = resolve_type_(stmt->var_type);
        local.wasm_name = "$v" + stmt->var_name;
        env.locals[stmt->var_name] = local;
        locals.push_back(local);
    }
    if (stmt->kind == StmtKind::If) {
        for (const auto &s : stmt->then_body) {
            CollectLocals(s, env, locals);
        }
        for (const auto &s : stmt->else_body) {
            CollectLocals(s, env, locals);
        }
    }
    if (stmt->kind == StmtKind::While) {
        for (const auto &s : stmt->body) {
            CollectLocals(s, env, locals);
        }
    }
}
