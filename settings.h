#pragma once
#include <map>
#include <string>
#include <obs-module.h>

using namespace std;
class Settings {
	map<string, string> settings;
	bool startMessageDisable = false;
public:
	void load();
	bool getStartMessageDisable();
	map<string, string> getMap();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
private:
	string settingsFilePath = "";
	void addToMap(string, string);
};
