#pragma once
#include <map>

using namespace std;
class Settings {
	map<string, string> settings;
public:
	void load();
	map<string, string> getMap();
	string getSettingsFilePath();
private:
	string settingsFilePath = "..\\..\\data\\obs-plugins\\SceneSwitcher\\settings.txt";
	void addToMap(string, string);
};
