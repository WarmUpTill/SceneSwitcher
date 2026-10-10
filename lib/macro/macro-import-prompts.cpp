#include "macro-import-prompts.hpp"

namespace advss {

static void savePlaceholders(obs_data_t *obj, const char *name,
			     const std::vector<std::string> &placeholders)
{
	obs_data_array_t *array = obs_data_array_create();
	for (auto &placeholder : placeholders) {
		obs_data_t *entry = obs_data_create();
		obs_data_set_string(entry, "value", placeholder.c_str());
		obs_data_array_push_back(array, entry);
		obs_data_release(entry);
	}
	obs_data_set_array(obj, name, array);
	obs_data_array_release(array);
}

static std::vector<std::string> loadPlaceholders(obs_data_t *obj,
						 const char *name)
{
	std::vector<std::string> result;
	obs_data_array_t *array = obs_data_get_array(obj, name);
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *entry = obs_data_array_item(array, i);
		result.emplace_back(obs_data_get_string(entry, "value"));
		obs_data_release(entry);
	}
	obs_data_array_release(array);
	return result;
}

void SaveImportPrompts(obs_data_t *obj,
		       const std::vector<ImportPrompt> &prompts)
{
	obs_data_array_t *array = obs_data_array_create();
	for (auto &prompt : prompts) {
		obs_data_t *entry = obs_data_create();
		obs_data_set_string(entry, "label", prompt.label.c_str());
		obs_data_set_int(entry, "type", static_cast<int>(prompt.type));
		savePlaceholders(entry, "placeholders", prompt.placeholders);
		obs_data_set_string(entry, "defaultValue",
				    prompt.defaultValue.c_str());
		obs_data_array_push_back(array, entry);
		obs_data_release(entry);
	}
	obs_data_set_array(obj, "importPrompts", array);
	obs_data_array_release(array);
}

std::vector<ImportPrompt> LoadImportPrompts(obs_data_t *obj)
{
	std::vector<ImportPrompt> result;
	obs_data_array_t *array = obs_data_get_array(obj, "importPrompts");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		obs_data_t *entry = obs_data_array_item(array, i);
		ImportPrompt prompt;
		prompt.label = obs_data_get_string(entry, "label");
		prompt.type = static_cast<ImportPromptType>(
			obs_data_get_int(entry, "type"));
		prompt.placeholders = loadPlaceholders(entry, "placeholders");
		prompt.defaultValue =
			obs_data_get_string(entry, "defaultValue");
		result.emplace_back(std::move(prompt));
		obs_data_release(entry);
	}
	obs_data_array_release(array);
	return result;
}

} // namespace advss
