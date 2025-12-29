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
#include <vector>

#include "codegen_types.h"

class FunctionEmitter {
public:
    using WasmTypeEmitter = std::function<std::string(const std::shared_ptr<Type> &)>;
    using TypeResolver = std::function<std::shared_ptr<Type>(const TypeSpec &)>;
    using StmtEmitter = std::function<void(const StmtPtr &, Env &)>;
    using ZeroEmitter = std::function<void(const std::shared_ptr<Type> &)>;

    FunctionEmitter(std::ostream &out,
                    WasmTypeEmitter wasm_type,
                    TypeResolver resolve_type,
                    StmtEmitter emit_stmt,
                    ZeroEmitter emit_zero);

    void EmitFunction(const Function &fn, const std::string &owner, const FunctionInfo &info);

private:
    std::ostream &out_;
    WasmTypeEmitter wasm_type_;
    TypeResolver resolve_type_;
    StmtEmitter emit_stmt_;
    ZeroEmitter emit_zero_;

    void CollectLocals(const StmtPtr &stmt, Env &env, std::vector<LocalInfo> &locals);
};
