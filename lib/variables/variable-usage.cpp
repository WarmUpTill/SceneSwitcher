#include "variable-usage.hpp"
#include "macro-helpers.hpp"
#include "macro.hpp"
#include "sync-helpers.hpp"
#include "variable-ref.hpp"

#include <obs.hpp>

namespace advss {

static std::unordered_map<std::string, int> collectReferences(Macro *macro)
{
	VariableRefCollector collector;
	OBSDataAutoRelease data = obs_data_create();
	macro->Save(data);
	return collector.Counts();
}

std::unordered_map<std::string, VariableUsage> GetVariableUsage()
{
	std::unordered_map<std::string, VariableUsage> result;
	auto lock = LockContext();
	// Nested macros are saved as part of their parent macro
	for (const auto &macro : GetTopLevelMacros()) {
		for (const auto &[name, count] :
		     collectReferences(macro.get())) {
			auto &usage = result[name];
			usage.referenceCount += count;
			usage.macroNames.emplace_back(macro->Name());
		}
	}
	return result;
}

std::unordered_map<std::string, int> GetVariablesUsedByMacro(Macro *macro)
{
	if (!macro) {
		return {};
	}

	return collectReferences(macro);
}

} // namespace advss
