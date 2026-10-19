#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace advss {

struct VariableUsage {
	int referenceCount = 0;
	std::vector<std::string> macroNames;
};

std::unordered_map<std::string, VariableUsage> GetVariableUsage();

} // namespace advss
