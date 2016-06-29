#include "switcher.h"
#include <obs-module.h>
#include <obs-data.h>
#include <fstream>
#include <string>
#include <sstream>
#include <boost/filesystem.hpp>

using namespace std;

//temporary 
bool oldSettingsLoaded = false;
//Swicthing done in here
Switcher *switcher = new Switcher();
//settings source
struct obs_source_info sceneSwitcherOptionsSource;
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
	if (hotkeyData != NULL) {
		name.append(".txt");
		//save hotkey data
		ofstream file;
		file.open(string(configPath).append(name), ofstream::trunc);
		//hotkeyData = obs_hotkey_save(pauseHotkeyId); 	//doesnt seem to work in obs_module_unload (hotkey data freed already (<- Jim))
		size_t num = obs_data_array_count(hotkeyData);
		for (int i = 0; i < num; i++) {
			string temp = obs_data_get_json(obs_data_array_item(hotkeyData, i));
			file << temp;
		}
		file.close();
	}
}

string loadConfigFile(string filename)
{
	ifstream settingsFile;
	settingsFile.open(string(configPath).append(filename));
	string value;
	if (settingsFile.is_open())
	{
		settingsFile.seekg(0, ios::end);
		value.reserve(settingsFile.tellg());
		settingsFile.seekg(0, ios::beg);
		value.assign((istreambuf_iterator<char>(settingsFile)), istreambuf_iterator<char>());
	}
	settingsFile.close();
	return value;
}

void loadKeybinding(string name, obs_data_array_t *hotkeyData, obs_hotkey_id hotkeyId) {
	string temp = loadConfigFile(name.append(".txt"));
	hotkeyData = obs_data_array_create();
	if (!temp.empty())
	{
		obs_data_array_insert(hotkeyData, 0, obs_data_create_from_json(temp.c_str()));
		obs_hotkey_load(hotkeyId, hotkeyData);
	}
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("SceneSwitcher", "en-US")

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

const char *sceneSwitcherOptionsGetName(void *type_data)
{
	UNUSED_PARAMETER(type_data);
	return "Scene Switcher Options";
}
void *sceneSwitcherOptionsCreate(obs_data_t *settings, obs_source_t *source) {
	UNUSED_PARAMETER(settings);
	UNUSED_PARAMETER(source);
	return (void*)switcher;
}
void sceneSwitcherOptionsDestroy(void *data) {
	UNUSED_PARAMETER(data);
}
uint32_t sceneSwitcherOptionsGetWidth(void *data)
{
	UNUSED_PARAMETER(data);
	return 0;
}
uint32_t sceneSwitcherOptionsGetHeight(void *data)
{
	UNUSED_PARAMETER(data);
	return 0;
}
void sceneSwitcherOptionsSourceGetDefaults(obs_data_t *settings)
{
	//load settings file
	string temp = loadConfigFile("settings.txt");
	//set values from file
	obs_data_apply(settings, obs_data_create_from_json(temp.c_str()));
}
bool loadOldSettings(obs_properties_t *props, obs_property_t *property, void *data)
{
	//read old settings
	string oldPath = "../../data/obs-plugins/SceneSwitcher/settings.txt";
	ifstream oldSettingsFile;
	oldSettingsFile.open(oldPath);
	string line;
	string value;
	//construct new settings format
	string newFormat = "{\n\t\"WindowList\": [\n";
	string valueBeginning = "\t\t{\n\t\t\t\"value\": \"";
	string valueEnd = "\t\t},\n";
	while (oldSettingsFile.good())
	{
		getline(oldSettingsFile, line);
		if (!line.empty()) {
				newFormat += valueBeginning + line + "\"\n" + valueEnd;
			}
	}
	oldSettingsFile.close();
	//remove trailing ',' and newline
	newFormat.pop_back();
	newFormat.pop_back();
	newFormat += "\n\t]\n}";
	//check if there are useful new settings
	if (newFormat != "{\n\t\"WindowList\": \n\t]\n}") {
		//write to new settings file
		ofstream file;
		file.open(string(configPath).append("settings.txt"));
		file << newFormat;
		file.close();
		//make sure not to overwrite the settings
		oldSettingsLoaded = true;
		if (switcher->getIsRunning()) {
			switcher->stop();
		}
		switcher->load();
		switcher->start();
	}
	return true;
}
obs_properties_t *sceneSwitcherOptionsSourceGetProperties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_editable_list(props,
		"WindowList", "Window Name",
		(enum obs_editable_list_type)0, "",
		NULL);
	obs_properties_add_button(props, "LoadOldSettings", "Load settings from old version (restart OBS after clicking this button)", &loadOldSettings);
	return props;
}
void sceneSwitcherOptionsSourceSave(void *data, obs_data_t *settings)
{
	//check if user chose to load old settings
	if (oldSettingsLoaded) {
		//oldSettingsLoaded = false;
	}
	else {
		//hang here if multiple instances of Scene Switcher Options are active (why?)
		ofstream settingsFile;
		settingsFile.open(string(configPath).append("settings.txt"), ofstream::trunc);
		settingsFile << obs_data_get_json(settings);
		settingsFile.close();
		if (switcher->getIsRunning())
			switcher->stop();
		switcher->load();
		switcher->start();
	}
}
void sceneSwitcherOptionsSourceLoad(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(data);
	//load settings file
	string temp = loadConfigFile("settings.txt");
	//set values from file
	obs_data_apply(settings, obs_data_create_from_json(temp.c_str()));
}

void sceneSwitcherOptionsSourceSetup()
{
	sceneSwitcherOptionsSource.id = "Scene Switcher Options";
	sceneSwitcherOptionsSource.type = OBS_SOURCE_TYPE_INPUT;
	sceneSwitcherOptionsSource.get_name = sceneSwitcherOptionsGetName;
	sceneSwitcherOptionsSource.create = sceneSwitcherOptionsCreate;
	sceneSwitcherOptionsSource.destroy = sceneSwitcherOptionsDestroy;
	sceneSwitcherOptionsSource.get_width = sceneSwitcherOptionsGetWidth;
	sceneSwitcherOptionsSource.get_height = sceneSwitcherOptionsGetHeight;
	sceneSwitcherOptionsSource.get_defaults = sceneSwitcherOptionsSourceGetDefaults; //create settings file if not present
	sceneSwitcherOptionsSource.get_properties = sceneSwitcherOptionsSourceGetProperties;
	sceneSwitcherOptionsSource.save = sceneSwitcherOptionsSourceSave; //save new settings to file
	sceneSwitcherOptionsSource.load = sceneSwitcherOptionsSourceLoad; //read settings file
	
	obs_register_source(&sceneSwitcherOptionsSource);
}

bool obs_module_load(void)
{	
	//set config path and check if config dir was set up
	configPath = obs_module_config_path("");
	boost::filesystem::create_directories(configPath);
	//register hotkey
	pauseHotkeyId = obs_hotkey_register_frontend(PAUSE_HOTKEY_NAME, "Toggle automatic scene switching", SceneSwitcherPauseHotkey, switcher);
	//optionsHotkeyId = obs_hotkey_register_frontend(OPTIONS_HOTKEY_NAME, "Open the Scene Switcher options menu", SceneSwitcherOptionsHotkey, switcher);
	//load hotkey binding if set already
	loadKeybinding(PAUSE_HOTKEY_NAME, pauseHotkeyData, pauseHotkeyId);
	//loadKeybinding(OPTIONS_HOTKEY_NAME, optionsHotkeyData, optionsHotkeyId);
	//load settings file
	switcher->setSettingsFilePath(configPath);
	switcher->firstLoad();
	//create options menu
	sceneSwitcherOptionsSourceSetup();
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
