#pragma once

#include "stdafx.h"
#include "switcher.h"
#include <obs-module.h>
#include <obs-ui.h>
#include <obs-data.h>
#include <fstream>

using namespace std;

//Swicthing done in here
Switcher *switcher = new Switcher();
//Hotkey
obs_hotkey_id hotkeyId;
obs_data_array_t *hotkeyData;
//path to config folder where to save the hotkeybinding (probably later the settings file)
string configPath;


OBS_DECLARE_MODULE()


void SceneSwitcherHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed)
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
	hotkeyData = obs_hotkey_save(hotkeyId);
}


bool obs_module_load(void)
{	
	//set config path
	configPath = obs_module_config_path("");

	//register hotkey
	hotkeyId = obs_hotkey_register_frontend("Scene Switcher", "Toggle automatic scene switching", SceneSwitcherHotkey, switcher);
	//load hotkey binding if set already (ONLY WORKING FOR A SINGLE HOTKEY AT THE MOMENT! CAREFUL!)
	ifstream file(obs_module_config_path("hotkey.txt"));
	if (file.is_open())
	{
		string temp;
		file.seekg(0, std::ios::end);
		temp.reserve(file.tellg());
		file.seekg(0, std::ios::beg);
		temp.assign((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
		hotkeyData = obs_data_array_create();
		obs_data_array_insert(hotkeyData, 0,obs_data_create_from_json(temp.c_str()));
		obs_hotkey_load(hotkeyId, hotkeyData);
	}

	//load settings file
	switcher->load();
	//start the switching thread
	switcher->start();
	return true;
	
}

void obs_module_unload(void) {

	//save settings (the only hotkey for now)
	wstring stemp = wstring(configPath.begin(), configPath.end());
	LPCWSTR sw = stemp.c_str();
	//check if config dir exists
	if (CreateDirectory(sw, NULL) ||
		ERROR_ALREADY_EXISTS == GetLastError())
	{
		//save hotkey data
		fstream file;
		file.open(obs_module_config_path("hotkey.txt"), fstream::out);
		//doesnt seem to work in obs_module_unload (hotkey data freed already (<- Jim))
		//hotkeyData = obs_hotkey_save(hotkeyId);
		int num = obs_data_array_count(hotkeyData);
		for (int i = 0; i < num; i++) {
			string temp = obs_data_get_json(obs_data_array_item(hotkeyData, i));
			file << temp;
		}

		file.close();
	}
	
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


