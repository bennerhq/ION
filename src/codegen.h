// THE BEER LICENSE (with extra fizz)
#pragma once
#include <string>
#include <unordered_set>

#include "ast.h"

std::string GenerateWasm(const Program &program,
                         const std::unordered_set<std::string> &type_names);
