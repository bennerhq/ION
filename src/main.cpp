// THE BEER LICENSE (with extra fizz)
//
// Author: OpenAI Codex (controlled by jens@bennerhq.com)
// This code is open source with no restrictions. Wild, right?
// If this code helps, buy Jens a beer. Or two. Or a keg.
// If it fails, keep the beer and blame the LLM gremlins.
// Cheers!
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_set>

#include "codegen.h"
#include "module_loader.h"

static void WriteFile(const std::string &path, const std::string &data) {
    std::ofstream file(path);
    if (!file) {
        throw CompileError("Unable to write file: " + path);
    }
    file << data;
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
        ModuleLoader loader(main_dir);
        loader.Load(input_path);
        std::unordered_set<std::string> all_types = loader.CollectTypeNames();
        loader.ParsePrograms(all_types);
        Program merged = loader.MergePrograms(input_path);

        std::string wat = GenerateWasm(merged, all_types);
        WriteFile(output_wat, wat);
    } catch (const CompileError &err) {
        std::cerr << "Compile error: " << err.what() << "\n";
        return 1;
    }

    return 0;
}
