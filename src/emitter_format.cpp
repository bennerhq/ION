// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include "emitter_format.h"

#include <cctype>

#include "common.h"

FormatEmitter::FormatEmitter(std::ostream &out,
                             const std::unordered_map<std::string, int64_t> &string_offsets,
                             ExprEmitter emit_expr,
                             ExprInferrer infer_expr,
                             TypeResolver resolve_type)
    : out_(out),
      string_offsets_(string_offsets),
      emit_expr_(std::move(emit_expr)),
      infer_expr_(std::move(infer_expr)),
      resolve_type_(std::move(resolve_type)) {}

bool FormatEmitter::NeedsFormat(const std::string &format) const {
    for (char c : format) {
        if (c == '%' || c == '\n') {
            return true;
        }
    }
    return false;
}

void FormatEmitter::EmitFormattedPrint(const std::string &format, const std::vector<ExprPtr> &args, Env &env, int line) {
    size_t arg_index = 1;
    std::string literal;
    auto flush_literal = [&]() {
        if (literal.empty()) {
            return;
        }
        auto it = string_offsets_.find(literal);
        if (it == string_offsets_.end()) {
            throw CompileError("Missing format literal in string table");
        }
        int64_t offset = it->second;
        out_ << "    i32.const " << (offset + 8) << "\n";
        out_ << "    i32.const " << literal.size() << "\n";
        out_ << "    call $write_bytes\n";
        literal.clear();
    };
    for (size_t i = 0; i < format.size(); ++i) {
        char c = format[i];
        if (c == '%' && i + 1 < format.size()) {
            char next = format[i + 1];
            if (next == '%') {
                literal.push_back('%');
                i++;
                continue;
            }
            flush_literal();
            if (arg_index >= args.size()) {
                throw CompileError("Not enough arguments for format string");
            }
            int precision = 6;
            bool has_precision = false;
            if ((next == 'r' || next == 'e') && i + 2 < format.size() && format[i + 2] == '{') {
                size_t j = i + 3;
                if (j >= format.size() || !std::isdigit(static_cast<unsigned char>(format[j]))) {
                    throw CompileError("Format precision requires digits");
                }
                precision = 0;
                while (j < format.size() && std::isdigit(static_cast<unsigned char>(format[j]))) {
                    precision = precision * 10 + (format[j] - '0');
                    j++;
                }
                if (j >= format.size() || format[j] != '}') {
                    throw CompileError("Format precision missing '}'");
                }
                has_precision = true;
                i = j;
            } else {
                i++;
            }

            ExprPtr arg = args[arg_index++];
            auto arg_type = emit_expr_(arg, env);
            if (next == 'i') {
                if (arg_type->kind != Type::Kind::Int) {
                    throw CompileError("Format %i expects int at line " + std::to_string(line));
                }
                out_ << "    call $print_i64_raw\n";
            } else if (next == 'r') {
                if (arg_type->kind != Type::Kind::Real) {
                    throw CompileError("Format %r expects real at line " + std::to_string(line));
                }
                if (has_precision) {
                    out_ << "    i32.const " << precision << "\n";
                    out_ << "    call $print_f64_prec\n";
                } else {
                    out_ << "    call $print_f64_raw\n";
                }
            } else if (next == 'e') {
                if (arg_type->kind != Type::Kind::Real) {
                    throw CompileError("Format %e expects real at line " + std::to_string(line));
                }
                out_ << "    i32.const " << precision << "\n";
                out_ << "    call $print_f64_sci\n";
            } else if (next == 'b') {
                if (arg_type->kind != Type::Kind::Bool) {
                    throw CompileError("Format %b expects bool at line " + std::to_string(line));
                }
                out_ << "    call $print_bool_raw\n";
            } else if (next == 's') {
                if (arg_type->kind != Type::Kind::String) {
                    throw CompileError("Format %s expects string at line " + std::to_string(line));
                }
                out_ << "    call $print_string_raw\n";
            } else {
                throw CompileError("Unsupported format specifier in print");
            }
            continue;
        }
        literal.push_back(c);
    }
    flush_literal();
    if (arg_index != args.size()) {
        throw CompileError("Too many arguments for format string");
    }
}

void FormatEmitter::EmitRuntimeFormatCall(const std::vector<ExprPtr> &args, Env &env, int line) {
    auto fmt_type = infer_expr_(args[0], env);
    if (fmt_type->kind != Type::Kind::String) {
        throw CompileError("print format must be string at line " + std::to_string(line));
    }
    size_t count = args.size() - 1;
    emit_expr_(args[0], env);
    out_ << "    local.set $tmp0\n";
    out_ << "    i64.const " << (count * 16) << "\n";
    out_ << "    call $alloc\n";
    out_ << "    local.set $tmp1\n";
    for (size_t i = 0; i < count; ++i) {
        const auto &arg = args[i + 1];
        auto arg_type = infer_expr_(arg, env);
        int tag = 0;
        if (arg_type->kind == Type::Kind::Int) {
            tag = 1;
        } else if (arg_type->kind == Type::Kind::Real) {
            tag = 2;
        } else if (arg_type->kind == Type::Kind::Bool) {
            tag = 3;
        } else if (arg_type->kind == Type::Kind::String) {
            tag = 4;
        } else {
            throw CompileError("Unsupported format argument type at line " + std::to_string(line));
        }
        int64_t offset = static_cast<int64_t>(i) * 16;
        out_ << "    local.get $tmp1\n";
        out_ << "    i64.const " << offset << "\n";
        out_ << "    i64.add\n";
        out_ << "    i32.wrap_i64\n";
        out_ << "    i32.const " << tag << "\n";
        out_ << "    i32.store\n";
        if (arg_type->kind == Type::Kind::Real) {
            emit_expr_(arg, env);
            out_ << "    local.set $tmpf\n";
            out_ << "    local.get $tmp1\n";
            out_ << "    i64.const " << (offset + 8) << "\n";
            out_ << "    i64.add\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmpf\n";
            out_ << "    f64.store\n";
        } else {
            emit_expr_(arg, env);
            out_ << "    local.set $tmp2\n";
            out_ << "    local.get $tmp1\n";
            out_ << "    i64.const " << (offset + 8) << "\n";
            out_ << "    i64.add\n";
            out_ << "    i32.wrap_i64\n";
            out_ << "    local.get $tmp2\n";
            out_ << "    i64.store\n";
        }
    }
    out_ << "    local.get $tmp0\n";
    out_ << "    local.get $tmp1\n";
    out_ << "    i32.const " << count << "\n";
    out_ << "    call $print_format\n";
}
