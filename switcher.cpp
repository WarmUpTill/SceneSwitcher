#pragma once
#include "stdafx.h"
#include "settings.h"
#include <map>
#include <windows.h>
#include <string>
#include <chrono>
#include <obs.h>
#include <thread>
#include <regex>
#include "switcher.h"

using namespace std;

//scene switching is done in here
void Switcher::switcherThreadFunc() {
	while (isRunning) {
		//get active window title
		string windowname = GetActiveWindowTitle();
		bool match = false;
		string name = "";
		for (std::map<string, string>::iterator iter = settingsMap.begin(); iter != settingsMap.end(); ++iter)
		{
			try
			{
				regex e = regex(iter->first);
				match = regex_match(windowname, e);
				if (match) {
					name = iter->second;
					break;
				}
			}
			catch(exception const & ex)
			{

			}
		}
		//do we know the window title or is a fullscreen/backup Scene set?
		if (!(settingsMap.find("Fullscreen Scene Name") == settingsMap.end()) || !(settingsMap.find("Backup Scene Name") == settingsMap.end())||match){

			if (!match && !(settingsMap.find("Fullscreen Scene Name") == settingsMap.end()) && isWindowFullscreen()) {
				name = settingsMap.find("Fullscreen Scene Name")->second;
			}
			else if (!match && !(settingsMap.find("Backup Scene Name") == settingsMap.end())) {
				name = settingsMap.find("Backup Scene Name")->second;
			}
				obs_source_t * transitionUsed = obs_get_output_source(0);
				obs_source_t * sceneUsed = obs_transition_get_active_source(transitionUsed);
				const char *sceneUsedName = obs_source_get_name(sceneUsed);
				//check if current scene is already the desired scene
				if ((sceneUsedName) && strcmp(sceneUsedName, name.c_str()) != 0) {
					//switch scene
					obs_source_t *source = obs_get_source_by_name(name.c_str());
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

void Switcher::firstLoad() {
	settings.load();
	settingsMap = settings.getMap();
	if (settingsMap.find("Disable Start Message") == settingsMap.end() || settingsMap.find("Disable Start Message")->second != "Yes") {
		string message = "The following settings were found for Scene Switcher:\n";
		for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
		{
			message += (it->first) + " -> " + it->second + "\n";
		}
		MessageBoxA(0, message.c_str(), "Scene Switcher", 0);
	}
}

//load the settings needed to start the thread
void Switcher::load() {
	settings.load();
	settingsMap = settings.getMap();
}

//start thread
void Switcher::start()
{
	isRunning = true;
	switcherThread = thread(&Switcher::switcherThreadFunc, this);
}

//checks if active window is in fullscreen
bool Switcher::isWindowFullscreen() {
	RECT appBounds;
	RECT rc;
	GetWindowRect(GetDesktopWindow(), &rc);
	HWND hwnd = GetForegroundWindow();
	//Check we haven't picked up the desktop or the shell
	if (hwnd != GetDesktopWindow() || hwnd != GetShellWindow())
	{
		GetWindowRect(hwnd, &appBounds);
		//determine if window is fullscreen
		if (rc.bottom == appBounds.bottom && rc.top == appBounds.top && rc.left == appBounds.left && rc.right == appBounds.right)
		{
			return true;
		}
	}
	return false;
}

//reads the title of the currently active window
string Switcher::GetActiveWindowTitle()
{
	char wnd_title[256];
	//get handle of currently active window
	HWND hwnd = GetForegroundWindow(); 
	GetWindowTextA(hwnd, wnd_title, sizeof(wnd_title));
	return wnd_title;
}

//tell the thread to stop
void Switcher::stop() {
	isRunning = false;
	switcherThread.join();
	return;
}


string Switcher::getSettingsFilePath()
{
	return settings.getSettingsFilePath();
}

bool Switcher::getIsRunning()
{
	return isRunning;
}