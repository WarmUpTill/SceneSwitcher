#pragma once
#include "settings.h"
#include <map>
#include <thread>

using namespace std;

class Switcher {
public:
	bool getIsRunning();
	void firstLoad();
	void load();
	void start();
	void stop();
	string getSettingsFilePath();
	thread switcherThread;
private:
	bool isRunning = true;
	Settings settings;
	map<string, string> settingsMap;
	void switcherThreadFunc();
	bool isWindowFullscreen();
	string GetActiveWindowTitle();
};