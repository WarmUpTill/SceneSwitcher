#include "switcher.h"
#include <obs-module.h>
#include <obs-data.h>
#include <fstream>
#include <string>
#include <boost/filesystem.hpp>

using namespace std;

//Swicthing done in here
Switcher *switcher = new Switcher();
//Hotkeys
const char *PAUSE_HOTKEY_NAME = "pauseHotkey";
const char *OPTIONS_HOTKEY_NAME = "optionsHotkey";
obs_hotkey_id pauseHotkeyId;
obs_hotkey_id optionsHotkeyId;
obs_data_array_t *pauseHotkeyData;
obs_data_array_t *optionsHotkeyData;
//path to config folder where to save the hotkeybinding (probably later the settings file)
string configPath;

//save settings (the only hotkey for now)
void saveKeybinding(string name, obs_data_array_t *hotkeyData) {
	name.append(".txt");

	//check if config dir exists
    if (boost::filesystem::create_directories(configPath)){
		//save hotkey data
		fstream file;
		file.open(obs_module_config_path(name.c_str()), fstream::out);
		//doesnt seem to work in obs_module_unload (hotkey data freed already (<- Jim))
		//hotkeyData = obs_hotkey_save(pauseHotkeyId);
		size_t num = obs_data_array_count(hotkeyData);
		for (int i = 0; i < num; i++) {
			string temp = obs_data_get_json(obs_data_array_item(hotkeyData, i));
			file << temp;
		}
	}
}

void loadKeybinding(string name, obs_data_array_t *hotkeyData, obs_hotkey_id hotkeyId) {
	name.append(".txt");
	ifstream file(obs_module_config_path(name.c_str()));
	if (file.is_open())
	{
		string temp;
		file.seekg(0, std::ios::end);
		temp.reserve(file.tellg());
		file.seekg(0, std::ios::beg);
		temp.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		hotkeyData = obs_data_array_create();
		if (!temp.empty()) {
			//fails if hotkey not set
			obs_data_array_insert(hotkeyData, 0, obs_data_create_from_json(temp.c_str()));
			obs_hotkey_load(hotkeyId, hotkeyData);
		}
	}
}

OBS_DECLARE_MODULE()


void SceneSwitcherPauseHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
	{
		Switcher *switcher = static_cast<Switcher *>(data);
		if (switcher->getIsRunning())
		{
			switcher->stop();
		}
		else
		{
			switcher->start();
		}
	}
	//save the keybinding here since it is currently not possible in obs_module_unload
	pauseHotkeyData = obs_hotkey_save(pauseHotkeyId);
}

bool obs_module_load(void)
{	
	//set config path
	configPath = obs_module_config_path("");
	//register hotkey
	pauseHotkeyId = obs_hotkey_register_frontend(PAUSE_HOTKEY_NAME, "Toggle automatic scene switching", SceneSwitcherPauseHotkey, switcher);
	//optionsHotkeyId = obs_hotkey_register_frontend(OPTIONS_HOTKEY_NAME, "Open the Scene Switcher options menu", SceneSwitcherOptionsHotkey, switcher);
	//load hotkey binding if set already
	loadKeybinding(PAUSE_HOTKEY_NAME, pauseHotkeyData, pauseHotkeyId);
	//loadKeybinding(OPTIONS_HOTKEY_NAME, optionsHotkeyData, optionsHotkeyId);
	//load settings file
	switcher->firstLoad();
	//start the switching thread
	switcher->start();
	return true;
}

void obs_module_unload(void) {
	switcher->stop();
	//save settings (only hotkey for now)
	saveKeybinding(PAUSE_HOTKEY_NAME, pauseHotkeyData);
	//saveKeybinding(OPTIONS_HOTKEY_NAME, optionsHotkeyData);
}

const char *obs_module_author(void)
{
	return "WarmUpTill";
}

const char *obs_module_name(void)
{
	return "Scene Switcher";
}

const char *obs_module_description(void)
{
	return "Automatic scene switching";
}
