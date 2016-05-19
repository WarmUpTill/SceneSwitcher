// DexterSceneSwitcher.cpp : Defines the exported functions for the DLL application.
//

#pragma once

#include "stdafx.h"
#include "switcher.h"
#include <obs-module.h>
#include <thread>


using namespace std;


OBS_DECLARE_MODULE()

bool running = false;
Switcher switcher;
thread switcherThread;

bool obs_module_load(void)
{	
	//maybe do some UI stuff?

	//void obs_register_modeless_ui(const struct obs_modeless_ui *info);

	//create thread
	running = true;
	map<string,string> s = switcher.load();
	switcherThread = thread(switcherThreadFunc,  s);
	return true;
	
}


void obs_module_unload(void)
{
	if(running)
{
	//tell the thread to stop working
	switcher.stop();
	switcherThread.join();
}
return;
}

const char *obs_module_author(void)
{
	return "WarmUpTill";
}

const char *obs_module_name(void)
{
	return "DexterSceneSwitcher";
}

const char *obs_module_description(void)
{
	return "SceneSwitching for Dexter";
}


