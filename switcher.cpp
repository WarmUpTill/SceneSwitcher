#pragma once


#include "stdafx.h"
#include "settings.h"
#include <iostream>
#include <map>
#include <windows.h>
#include <string>
#include <chrono>
#include <obs.h>



using namespace std;

//is the thread running? see below :D
static bool isRunning = true;
string GetActiveWindowTitle();

class Switcher {
public:
	map<string, string> load();
	void stop();
private:
	Settings settings;
	map<string, string> settingsMap;
};

//scene switching is done in here
void switcherThreadFunc(map<string, string> settingsMap) {

	string windowname = "";

	while (isRunning) {

		//get active window title
		windowname = GetActiveWindowTitle();

		//do we know the window title?
		if (!(settingsMap.find(windowname) == settingsMap.end())) {

			//check if current scene is already the desired scene
			const char * name = settingsMap.find(windowname)->second.c_str();

			//remember to release all sources
			obs_source_t * transitionUsed = obs_get_output_source(0);
			obs_source_t * sceneUsed = obs_transition_get_active_source(transitionUsed);
			const char * sceneUsedName = obs_source_get_name(sceneUsed);

			//NOTE: transitions seem to be scenes and will be canceled unless we check if the current scene is a transition
			if ((sceneUsedName) && strcmp(sceneUsedName,name) != 0) { 
				//switch scene
				obs_source_t *source = obs_get_source_by_name(name);
				if (source == NULL)
				{
					//warn("Source not found: \"%s\"", name);
					;
				}
				else if (obs_scene_from_source(source) == NULL)
				{
					//warn("\"%s\" is not a scene", name);
					;
				}
				else {
					//create transition to new scene (otherwise UI wont work anymore)
					//OBS_TRANSITION_MODE_AUTO uses the obs user settings for transitions
					obs_transition_start(transitionUsed, OBS_TRANSITION_MODE_AUTO, 300, source);
				}
				obs_source_release(source);
			}
			obs_source_release(sceneUsed); 
			obs_source_release(transitionUsed);
		}
		//sleep for a bit
		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}


//load the settings needed to start the thread
map<string, string> Switcher::load() {
	//settings file in 
	string settingsFilePath = "..\\..\\data\\obs-plugins\\DexterSceneSwitcher\\settings.txt"; //"..\\..\\data\\obs-plugins\\DexterSceneSwitcher\\settings.txt";
	settings.load(settingsFilePath);
	map<string,string> settingsMap = settings.getMap();
	isRunning = true;
	string message = "The following settings were found for Scene Switcher:\n";
	for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
	{
		message += (it->first) + " -> " + it->second + "\n";
	}
	MessageBox(0, message.c_str(), "Scene Switcher", 0);
	return settingsMap;
	
}

//reads the title of the currently active window
string GetActiveWindowTitle()
{
	char wnd_title[256];
	HWND hwnd = GetForegroundWindow(); // get handle of currently active window
	GetWindowTextA(hwnd, wnd_title, sizeof(wnd_title));
	return wnd_title;
}

//tell the thread to stop
void Switcher::stop() {
	isRunning = false; //isRunning seems to be called randomly and disables the plugin (?)
	return;
}
