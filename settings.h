#pragma once
#include <map>
#include <vector>
#include <string>

using namespace std;

struct Data {
	string sceneName = "";
	bool isFullscreen = false;
};

class Settings {
public:
	void load();
	bool getStartMessageDisable();
	map<string, Data> getMap();
	pair<vector<string>, vector<string>> getSceneRoundTrip();
	vector<string> getPauseScenes();
	vector<string> getIgnoreNames();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
private:
	string settingsFilePath = "";
	map<string, Data> settings;
	pair<vector<string>, vector<string>> sceneRoundTrip;
	vector<string> pauseScenes;
	vector<string> ignoreNames;
	bool startMessageDisable = false;
};
