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
#include <string>

#include "codegen_types.h"

class AssignmentEmitter {
public:
    using ExprEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeSizer = std::function<int64_t(const std::shared_ptr<Type> &)>;
    using IndexAddressEmitter = std::function<std::shared_ptr<Type>(const ExprPtr &, Env &)>;
    using TypeChecker = std::function<void(const std::shared_ptr<Type> &,
                                           const std::shared_ptr<Type> &,
                                           int)>;
    using StructLookup = std::function<const StructInfo *(const std::string &)>;

    AssignmentEmitter(std::ostream &out,
                      ExprEmitter emit_expr,
                      TypeSizer type_size,
                      TypeChecker type_check,
                      StructLookup lookup_struct,
                      IndexAddressEmitter emit_index_address);

    void EmitAssignment(const ExprPtr &target, const ExprPtr &value, Env &env);

private:
    std::ostream &out_;
    ExprEmitter emit_expr_;
    TypeSizer type_size_;
    TypeChecker type_check_;
    StructLookup lookup_struct_;
    IndexAddressEmitter emit_index_address_;

    std::shared_ptr<Type> EmitAddress(const ExprPtr &expr, Env &env);
    void EmitStore(const std::shared_ptr<Type> &type);
};
