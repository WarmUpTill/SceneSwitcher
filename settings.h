#pragma once
#include <map>
#include <string>

using namespace std;
class Settings {
	map<string, string> settings;
public:
	void load();
	map<string, string> getMap();
	string getSettingsFilePath();
private:
	string settingsFilePath = "/Users/Till/Desktop/SceneSwitcher/settings.txt";
	void addToMap(string, string);
};
