// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
//
// Cheers!

#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ast.h"

struct ModuleData {
    std::string name;
    std::string path;
    std::vector<Token> tokens;
    std::vector<ImportDecl> imports;
    std::unordered_set<std::string> struct_names;
    Program program;
};

class ModuleLoader {
public:
    explicit ModuleLoader(const std::string &main_dir);
    void Load(const std::string &input_path);
    std::unordered_set<std::string> CollectTypeNames() const;
    void ParsePrograms(const std::unordered_set<std::string> &all_types);
    Program MergePrograms(const std::string &input_path);

private:
    std::string main_dir_;
    std::unordered_map<std::string, ModuleData> modules_;
    std::unordered_map<std::string, std::string> module_name_to_path_;

    static std::string ReadFile(const std::string &path);
    static bool FileExists(const std::string &path);
    static std::string JoinPath(const std::string &base, const std::string &path);
    static std::string ModuleToPath(const std::string &module);
    static bool IsAbsolutePath(const std::string &path);
    static bool EndsWith(const std::string &value, const std::string &suffix);
    static std::string BasenameNoExt(const std::string &path);
    static std::string ModuleIdForImport(const ImportDecl &decl);

    std::string ResolveModulePath(const ImportDecl &decl) const;
    void LoadModule(const std::string &path, const std::string &module_id);

    static std::vector<ImportDecl> ScanImports(const std::vector<Token> &tokens);
    static std::unordered_set<std::string> ScanStructNames(const std::vector<Token> &tokens);

    static bool BuildFieldChain(const ExprPtr &expr, std::vector<std::string> &parts);
    static void RewriteExpr(const ExprPtr &expr, const std::string &module_name, bool qualify_local,
                            const std::unordered_set<std::string> &local_functions,
                            const std::unordered_map<std::string, std::string> &import_aliases);
    static void RewriteStmt(const StmtPtr &stmt, const std::string &module_name, bool qualify_local,
                            const std::unordered_set<std::string> &local_functions,
                            const std::unordered_map<std::string, std::string> &import_aliases);
    static void RewriteProgramCalls(Program &program, const std::string &module_name, bool qualify_local,
                                    const std::unordered_set<std::string> &local_functions,
                                    const std::unordered_map<std::string, std::string> &import_aliases);
};
