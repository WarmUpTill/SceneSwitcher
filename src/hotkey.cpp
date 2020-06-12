#include <obs-module.h>
#include "headers/advanced-scene-switcher.hpp"

void startHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
		     bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (!(switcher->th && switcher->th->isRunning()))
			switcher->Start();
	}

	obs_data_array *hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL) {
		char *path = obs_module_config_path("");
		std::ofstream file;
		file.open(std::string(path).append(START_HOTKEY_PATH),
			  std::ofstream::trunc);
		if (file.is_open()) {
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++) {
				obs_data_t *data =
					obs_data_array_item(hotkeyData, i);
				std::string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

void stopHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
		    bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (switcher->th && switcher->th->isRunning())
			switcher->Stop();
	}

	obs_data_array *hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL) {
		char *path = obs_module_config_path("");
		std::ofstream file;
		file.open(std::string(path).append(STOP_HOTKEY_PATH),
			  std::ofstream::trunc);
		if (file.is_open()) {
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++) {
				obs_data_t *data =
					obs_data_array_item(hotkeyData, i);
				std::string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

void startStopToggleHotkeyFunc(void *data, obs_hotkey_id id,
			       obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (switcher->th && switcher->th->isRunning())
			switcher->Stop();
		else
			switcher->Start();
	}

	obs_data_array *hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL) {
		char *path = obs_module_config_path("");
		std::ofstream file;
		file.open(std::string(path).append(TOGGLE_HOTKEY_PATH),
			  std::ofstream::trunc);
		if (file.is_open()) {
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++) {
				obs_data_t *data =
					obs_data_array_item(hotkeyData, i);
				std::string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

std::string loadConfigFile(std::string filename)
{
	std::ifstream settingsFile;
	char *path = obs_module_config_path("");
	std::string value;

	settingsFile.open(std::string(path).append(filename));
	if (settingsFile.is_open()) {
		settingsFile.seekg(0, std::ios::end);
		value.reserve(settingsFile.tellg());
		settingsFile.seekg(0, std::ios::beg);
		value.assign((std::istreambuf_iterator<char>(settingsFile)),
			     std::istreambuf_iterator<char>());
		settingsFile.close();
	}
	bfree(path);
	return value;
}

void loadKeybinding(obs_hotkey_id hotkeyId, std::string path)
{
	std::string bindings = loadConfigFile(path);
	if (!bindings.empty()) {
		obs_data_array_t *hotkeyData = obs_data_array_create();
		obs_data_t *data = obs_data_create_from_json(bindings.c_str());
		obs_data_array_insert(hotkeyData, 0, data);
		obs_data_release(data);
		obs_hotkey_load(hotkeyId, hotkeyData);
		obs_data_array_release(hotkeyData);
	}
}
