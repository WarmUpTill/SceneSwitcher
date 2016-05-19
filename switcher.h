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
	map<string, string> load();
	void stop();
private:
	Settings settings;
	map<string, string> settingsMap;
};


string GetActiveWindowTitle();
void switcherThreadFunc(map<string, string> settingsMap);
