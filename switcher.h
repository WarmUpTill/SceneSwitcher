#pragma once
#include "settings.h"
#include <iostream>
#include <map>
#include <windows.h>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

class Switcher {
public:
	bool getIsRunning();
	void load();
	void start();
	void stop();
	thread switcherThread;
private:
	bool isRunning = true;
	Settings settings;
	map<string, string> settingsMap;
	void switcherThreadFunc();
	bool isWindowFullscreen();
	string GetActiveWindowTitle();
};