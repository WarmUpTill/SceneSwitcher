#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace advss {

class Macro;

struct VariableUsage {
	int referenceCount = 0;
	std::vector<std::string> macroNames;
};

std::unordered_map<std::string, VariableUsage> GetVariableUsage();
std::unordered_map<std::string, int> GetVariablesUsedByMacro(Macro *macro);
std::unordered_map<std::string, int>
GetVariablesUsedByMacros(const std::vector<std::shared_ptr<Macro>> &macros);

} // namespace advss
