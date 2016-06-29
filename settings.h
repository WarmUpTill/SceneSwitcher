#pragma once
#include <map>
#include <string>
#include <obs-module.h>

using namespace std;

struct Data {
	string sceneName = "";
	bool isFullscreen = false;
};

class Settings {
	map<string, Data> settings;
	bool startMessageDisable = false;
public:
	void load();
	bool getStartMessageDisable();
	map<string, Data> getMap();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
private:
	string settingsFilePath = "";
	void addToMap(string, Data);
};
