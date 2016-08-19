#pragma once
#include "settings.h"
#include <map>
#include <vector>
#include <thread>

using namespace std;

class Switcher {
public:
	bool getIsRunning();
	void firstLoad();
	void load();
	void start();
	void stop();
	void openUI();
	string getSettingsFilePath();
	void setSettingsFilePath(string path);
	thread switcherThread;
	~Switcher();
private:
	bool isRunning = true;
	Settings settings;
	map<string, Data> settingsMap;
	pair<vector<string>, vector<string>> sceneRoundTrip;
	vector<string> pauseScenes;
	vector<string> ignoreNames;
	void switcherThreadFunc();
	bool isWindowFullscreen();
	string GetActiveWindowTitle();
	pair<int, int> getCursorXY();
};
