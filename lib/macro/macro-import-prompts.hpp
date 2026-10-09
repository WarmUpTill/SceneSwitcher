#pragma once

#include "export-symbol-helper.hpp"

#include <obs-data.h>
#include <string>
#include <vector>

namespace advss {

enum class ImportPromptType {
	File,
	Folder,
	Scene,
	Source,
	String,
	Variable,
};

struct ImportPrompt {
	std::string label;
	ImportPromptType type = ImportPromptType::String;
	std::vector<std::string> placeholders;
	std::string defaultValue;
};

EXPORT void SaveImportPrompts(obs_data_t *obj,
			      const std::vector<ImportPrompt> &prompts);
EXPORT std::vector<ImportPrompt> LoadImportPrompts(obs_data_t *obj);

} // namespace advss
