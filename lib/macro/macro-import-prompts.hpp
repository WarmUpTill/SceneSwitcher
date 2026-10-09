#pragma once

#include "export-symbol-helper.hpp"
#include "value-input.hpp"

#include <obs-data.h>
#include <string>
#include <vector>

namespace advss {

struct ImportPrompt {
	std::string label;
	ValueInputType type = ValueInputType::String;
	std::vector<std::string> placeholders;
	std::string defaultValue;
	std::vector<std::string> options;
};

EXPORT void SaveImportPrompts(obs_data_t *obj,
			      const std::vector<ImportPrompt> &prompts);
EXPORT std::vector<ImportPrompt> LoadImportPrompts(obs_data_t *obj);

} // namespace advss
