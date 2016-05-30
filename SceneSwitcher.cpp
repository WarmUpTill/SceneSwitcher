#pragma once

#include "stdafx.h"
#include "switcher.h"
#include <obs-module.h>
#include <obs-ui.h>
//#include <QtWidgets/QApplication>

Switcher *switcher = new Switcher();
using namespace std;


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
}


bool obs_module_load(void)
{	
	//modul UI not implemented yet
	//obs_modal_ui *uiInfo = new obs_modal_ui;
	//uiInfo->id = "SceneSwitcherUI";
	//uiInfo->task = "Modify settings for SceneSwitcher";
	//uiInfo->target = "qt";
	//uiInfo->exec = execute;
	//uiInfo->type_data = NULL; //<- sceneswitcher?? pause it? then reload settings?
	//uiInfo->free_type_data = NULL;

	//obs_register_modal_ui(uiInfo);

	//Hotkey
	obs_hotkey_register_frontend("Scene Switcher", "Toggle automatic scene switching", SceneSwitcherHotkey, switcher);
	switcher->load();
	switcher->start();
	
	return true;
	
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


