#pragma once
#include <map>
#include <string>
#include <obs-module.h>

using namespace std;
class Settings {
	map<string, string> settings;
public:
	void load();
	map<string, string> getMap();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
private:
	string settingsFilePath = "";
	void addToMap(string, string);
};
