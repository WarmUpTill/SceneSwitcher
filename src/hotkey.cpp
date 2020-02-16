#include <obs-module.h>
#include "advanced-scene-switcher.hpp"

void startHotkeyFunc(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
	{
		if (!switcher->th.joinable())
			switcher->Start();
	}

	obs_data_array* hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL)
	{
		char* path = obs_module_config_path("");
		ofstream file;
		file.open(string(path).append(START_HOTKEY_PATH), ofstream::trunc);
		if (file.is_open())
		{
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++)
			{
				obs_data_t* data = obs_data_array_item(hotkeyData, i);
				string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

void stopHotkeyFunc(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
	{
		if (switcher->th.joinable())
			switcher->Stop();
	}

	obs_data_array* hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL)
	{
		char* path = obs_module_config_path("");
		ofstream file;
		file.open(string(path).append(STOP_HOTKEY_PATH), ofstream::trunc);
		if (file.is_open())
		{
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++)
			{
				obs_data_t* data = obs_data_array_item(hotkeyData, i);
				string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

void startStopToggleHotkeyFunc(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
	{
		if (switcher->th.joinable())
			switcher->Stop();
		else
			switcher->Start();
	}

	obs_data_array* hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL)
	{
		char* path = obs_module_config_path("");
		ofstream file;
		file.open(string(path).append(TOGGLE_HOTKEY_PATH), ofstream::trunc);
		if (file.is_open())
		{
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++)
			{
				obs_data_t* data = obs_data_array_item(hotkeyData, i);
				string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

string loadConfigFile(string filename)
{
	ifstream settingsFile;
	char* path = obs_module_config_path("");
	string value;

	settingsFile.open(string(path).append(filename));
	if (settingsFile.is_open())
	{
		settingsFile.seekg(0, ios::end);
		value.reserve(settingsFile.tellg());
		settingsFile.seekg(0, ios::beg);
		value.assign((istreambuf_iterator<char>(settingsFile)), istreambuf_iterator<char>());
		settingsFile.close();
	}
	bfree(path);
	return value;
}

void loadKeybinding(obs_hotkey_id hotkeyId, string path)
{
	string bindings = loadConfigFile(path);
	if (!bindings.empty())
	{
		obs_data_array_t* hotkeyData = obs_data_array_create();
		obs_data_t* data = obs_data_create_from_json(bindings.c_str());
		obs_data_array_insert(hotkeyData, 0, data);
		obs_data_release(data);
		obs_hotkey_load(hotkeyId, hotkeyData);
		obs_data_array_release(hotkeyData);
	}
}
