#pragma once
#include <map>
#include <vector>
#include <string>
#include <obs-module.h>

using namespace std;

struct Data {
	string sceneName = "";
	bool isFullscreen = false;
};

class Settings {
	vector<string> sceneRoundTrip;
	map<string, Data> settings;
	bool startMessageDisable = false;
public:
	void load();
	bool getStartMessageDisable();
	map<string, Data> getMap();
	vector<string> getSceneRoundTrip();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
private:
	string settingsFilePath = "";
};
