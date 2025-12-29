// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "codegen.h"
#include "lexer.h"
#include "parser.h"

static std::string ReadFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw CompileError("Unable to open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

static void WriteFile(const std::string &path, const std::string &data) {
    std::ofstream file(path);
    if (!file) {
        throw CompileError("Unable to write file: " + path);
    }
    file << data;
}

static bool FileExists(const std::string &path) {
    std::ifstream file(path);
    return file.good();
}

static std::string GetDirname(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) {
        return ".";
    }
    if (pos == 0) {
        return "/";
    }
    return path.substr(0, pos);
}

static std::string JoinPath(const std::string &base, const std::string &path) {
    if (base.empty() || base == ".") {
        return path;
    }
    if (base.back() == '/' || base.back() == '\\') {
        return base + path;
    }
    return base + "/" + path;
}

static std::string ModuleToPath(const std::string &module) {
    std::string path = module;
    std::replace(path.begin(), path.end(), '.', '/');
    path += ".ion";
    return path;
}

static bool IsAbsolutePath(const std::string &path) {
    return !path.empty() && path[0] == '/';
}

static bool EndsWith(const std::string &value, const std::string &suffix) {
    if (value.size() < suffix.size()) {
        return false;
    }
    return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string BasenameNoExt(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    std::string base = (pos == std::string::npos) ? path : path.substr(pos + 1);
    size_t dot = base.rfind('.');
    if (dot != std::string::npos) {
        base = base.substr(0, dot);
    }
    return base;
}

static std::string ModuleIdForImport(const ImportDecl &decl) {
    if (!decl.is_path) {
        return decl.module;
    }
    return BasenameNoExt(decl.module);
}

static std::string ResolveModulePath(const ImportDecl &decl, const std::string &main_dir) {
    std::string rel;
    if (decl.is_path) {
        rel = decl.module;
        if (!EndsWith(rel, ".ion")) {
            rel += ".ion";
        }
    } else {
        rel = ModuleToPath(decl.module);
    }
    std::string full = rel;
    if (!IsAbsolutePath(rel)) {
        full = JoinPath(main_dir, rel);
    }
    if (FileExists(full)) {
        return full;
    }
    throw CompileError("Unable to resolve module '" + decl.module + "'");
}

static std::vector<ImportDecl> ScanImports(const std::vector<Token> &tokens) {
    std::vector<ImportDecl> imports;
    bool at_line_start = true;
    int indent_level = 0;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const Token &tok = tokens[i];
        if (tok.type == TokenType::Indent) {
            indent_level++;
        } else if (tok.type == TokenType::Dedent) {
            indent_level = std::max(0, indent_level - 1);
        }
        if (tok.type == TokenType::Newline) {
            at_line_start = true;
            continue;
        }
        if (at_line_start && indent_level == 0 && tok.type == TokenType::Keyword && tok.text == "import") {
            ImportDecl decl;
            decl.line = tok.line;
            i++;
            if (i >= tokens.size()) {
                throw CompileError("Expected module name after import at line " + std::to_string(tok.line));
            }
            if (tokens[i].type == TokenType::String) {
                decl.module = tokens[i].text;
                decl.is_path = true;
            } else {
                if (tokens[i].type != TokenType::Identifier) {
                    throw CompileError("Expected module name after import at line " + std::to_string(tok.line));
                }
                std::string module = tokens[i].text;
                while (i + 2 < tokens.size() && tokens[i + 1].type == TokenType::Dot &&
                       tokens[i + 2].type == TokenType::Identifier) {
                    module += ".";
                    module += tokens[i + 2].text;
                    i += 2;
                }
                decl.module = module;
            }
            if (i + 2 < tokens.size() && tokens[i + 1].type == TokenType::Keyword &&
                tokens[i + 1].text == "as" && tokens[i + 2].type == TokenType::Identifier) {
                decl.alias = tokens[i + 2].text;
                i += 2;
            }
            imports.push_back(decl);
            at_line_start = false;
            continue;
        }
        at_line_start = false;
    }
    return imports;
}

static std::unordered_set<std::string> ScanStructNames(const std::vector<Token> &tokens) {
    std::unordered_set<std::string> names;
    size_t idx = 0;
    while (idx < tokens.size()) {
        size_t line_start = idx;
        while (idx < tokens.size() && tokens[idx].type != TokenType::Newline &&
               tokens[idx].type != TokenType::EndOfFile) {
            idx++;
        }
        size_t line_end = idx;
        if (line_end > line_start) {
            size_t first = line_start;
            while (first < line_end &&
                   (tokens[first].type == TokenType::Indent || tokens[first].type == TokenType::Dedent)) {
                first++;
            }
            bool has_paren = false;
            bool has_colon = false;
            for (size_t i = line_start; i < line_end; ++i) {
                if (tokens[i].type == TokenType::LParen) {
                    has_paren = true;
                    break;
                }
                if (tokens[i].type == TokenType::Colon) {
                    has_colon = true;
                }
            }
            if (has_colon && !has_paren && first < line_end && tokens[first].type == TokenType::Identifier) {
                names.insert(tokens[first].text);
            }
        }
        if (idx < tokens.size() && tokens[idx].type == TokenType::Newline) {
            idx++;
        } else if (idx < tokens.size() && tokens[idx].type == TokenType::EndOfFile) {
            break;
        }
    }
    return names;
}

static bool BuildFieldChain(const ExprPtr &expr, std::vector<std::string> &parts) {
    if (!expr) {
        return false;
    }
    if (expr->kind == ExprKind::Var) {
        parts.push_back(expr->text);
        return true;
    }
    if (expr->kind == ExprKind::Field) {
        if (!BuildFieldChain(expr->base, parts)) {
            return false;
        }
        parts.push_back(expr->field);
        return true;
    }
    return false;
}

static void RewriteExpr(const ExprPtr &expr, const std::string &module_name, bool qualify_local,
                        const std::unordered_set<std::string> &local_functions,
                        const std::unordered_map<std::string, std::string> &import_aliases) {
    if (!expr) {
        return;
    }
    RewriteExpr(expr->left, module_name, qualify_local, local_functions, import_aliases);
    RewriteExpr(expr->right, module_name, qualify_local, local_functions, import_aliases);
    RewriteExpr(expr->base, module_name, qualify_local, local_functions, import_aliases);
    RewriteExpr(expr->new_size, module_name, qualify_local, local_functions, import_aliases);
    for (const auto &arg : expr->args) {
        RewriteExpr(arg, module_name, qualify_local, local_functions, import_aliases);
    }
    if (expr->kind != ExprKind::Call || !expr->base) {
        return;
    }
    if (expr->base->kind == ExprKind::Var) {
        std::string name = expr->base->text;
        if (qualify_local && local_functions.count(name) > 0) {
            expr->base->text = module_name + "." + name;
        }
        return;
    }
    std::vector<std::string> parts;
    if (!BuildFieldChain(expr->base, parts) || parts.size() < 2) {
        return;
    }
    std::string module_path;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        if (i > 0) {
            module_path += ".";
        }
        module_path += parts[i];
    }
    auto it = import_aliases.find(module_path);
    if (it == import_aliases.end()) {
        return;
    }
    std::string qualified = it->second + "." + parts.back();
    auto node = std::make_shared<Expr>();
    node->kind = ExprKind::Var;
    node->text = qualified;
    node->line = expr->line;
    expr->base = node;
}

static void RewriteStmt(const StmtPtr &stmt, const std::string &module_name, bool qualify_local,
                        const std::unordered_set<std::string> &local_functions,
                        const std::unordered_map<std::string, std::string> &import_aliases) {
    if (!stmt) {
        return;
    }
    RewriteExpr(stmt->expr, module_name, qualify_local, local_functions, import_aliases);
    RewriteExpr(stmt->target, module_name, qualify_local, local_functions, import_aliases);
    for (const auto &child : stmt->then_body) {
        RewriteStmt(child, module_name, qualify_local, local_functions, import_aliases);
    }
    for (const auto &child : stmt->else_body) {
        RewriteStmt(child, module_name, qualify_local, local_functions, import_aliases);
    }
    for (const auto &child : stmt->body) {
        RewriteStmt(child, module_name, qualify_local, local_functions, import_aliases);
    }
}

static void RewriteProgramCalls(Program &program, const std::string &module_name, bool qualify_local,
                                const std::unordered_set<std::string> &local_functions,
                                const std::unordered_map<std::string, std::string> &import_aliases) {
    for (auto &fn : program.functions) {
        for (const auto &stmt : fn.body) {
            RewriteStmt(stmt, module_name, qualify_local, local_functions, import_aliases);
        }
    }
    for (auto &def : program.structs) {
        for (auto &method : def.methods) {
            for (const auto &stmt : method.body) {
                RewriteStmt(stmt, module_name, qualify_local, local_functions, import_aliases);
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: ionc <input.ion> [-o output.wat]\n";
        return 1;
    }
    std::string input_path = argv[1];
    std::string output_wat = "output.wat";
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_wat = argv[++i];
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }
    try {
        std::string main_dir = GetDirname(input_path);
        struct ModuleData {
            std::string name;
            std::string path;
            std::vector<Token> tokens;
            std::vector<ImportDecl> imports;
            std::unordered_set<std::string> struct_names;
            Program program;
        };

        std::unordered_map<std::string, ModuleData> modules;
        std::unordered_map<std::string, std::string> module_name_to_path;

        auto load_module = [&](const std::string &path, const std::string &module_id,
                               auto &&load_ref) -> void {
            auto it = modules.find(path);
            if (it != modules.end()) {
                return;
            }
            if (!module_id.empty()) {
                auto mit = module_name_to_path.find(module_id);
                if (mit != module_name_to_path.end() && mit->second != path) {
                    throw CompileError("Module name '" + module_id + "' resolves to multiple paths");
                }
                module_name_to_path[module_id] = path;
            }
            std::string source = ReadFile(path);
            Lexer lexer(source);
            auto tokens = lexer.Tokenize();
            ModuleData data;
            data.name = module_id;
            data.path = path;
            data.tokens = tokens;
            data.imports = ScanImports(tokens);
            data.struct_names = ScanStructNames(tokens);
            modules[path] = data;
            for (const auto &imp : data.imports) {
                std::string module_id = ModuleIdForImport(imp);
                std::string resolved = ResolveModulePath(imp, main_dir);
                load_ref(resolved, module_id, load_ref);
            }
        };

        load_module(input_path, "", load_module);

        std::unordered_set<std::string> all_types = {"int", "real", "bool", "string"};
        for (const auto &entry : modules) {
            all_types.insert(entry.second.struct_names.begin(), entry.second.struct_names.end());
        }

        for (auto &entry : modules) {
            Parser parser(entry.second.tokens, all_types);
            entry.second.program = parser.ParseProgram();
        }

        Program merged;
        std::unordered_set<std::string> struct_names;
        std::unordered_set<std::string> function_names;

        std::vector<ModuleData *> ordered;
        auto root_it = modules.find(input_path);
        if (root_it != modules.end()) {
            ordered.push_back(&root_it->second);
        }
        for (auto &entry : modules) {
            if (entry.first == input_path) {
                continue;
            }
            ordered.push_back(&entry.second);
        }

        for (auto *module : ordered) {
            bool is_root = module->path == input_path;
            std::unordered_map<std::string, std::string> alias_map;
            for (const auto &imp : module->program.imports) {
                std::string module_id = ModuleIdForImport(imp);
                std::string alias = imp.alias.empty() ? module_id : imp.alias;
                alias_map[alias] = module_id;
            }
            std::unordered_set<std::string> local_functions;
            for (const auto &fn : module->program.functions) {
                local_functions.insert(fn.name);
            }
            if (!is_root) {
                for (auto &fn : module->program.functions) {
                    fn.name = module->name + "." + fn.name;
                }
            }
            RewriteProgramCalls(module->program, module->name, !is_root, local_functions, alias_map);

            for (const auto &def : module->program.structs) {
                if (struct_names.count(def.name) > 0) {
                    throw CompileError("Duplicate struct name '" + def.name + "'");
                }
                struct_names.insert(def.name);
                merged.structs.push_back(def);
            }
            for (const auto &fn : module->program.functions) {
                if (function_names.count(fn.name) > 0) {
                    throw CompileError("Duplicate function name '" + fn.name + "'");
                }
                function_names.insert(fn.name);
                merged.functions.push_back(fn);
            }
        }

        std::string wat = GenerateWasm(merged, all_types);
        WriteFile(output_wat, wat);
    } catch (const CompileError &err) {
        std::cerr << "Compile error: " << err.what() << "\n";
        return 1;
    }

    return 0;
}
