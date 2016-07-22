#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include "settings.h"

using namespace std;

string Settings::getSettingsFilePath()
{
	return settingsFilePath;
}

void Settings::setSettingsFilePath(string path)
{
	settingsFilePath = path.append("settings.txt");
}

void Settings::load() {
	//reset the settings
	settings = map<string, Data>();
	sceneRoundTrip = vector<string>();
	pauseScenes = vector<string>();
	ignoreNames = vector<string>();
	//read the settings file
	vector<string> settingsElements;
	ifstream infile(settingsFilePath);
	string value;
	string line;
	size_t pos;
	int numValues;
	bool startMessageDisableFound = false;
	infile.seekg(0);
	while (infile.good())
	{
		pos = line.npos;
		numValues = 0;
		//read json file
		getline(infile, line);
		//disable the start message?
		if (!startMessageDisableFound) {
			pos = line.find("\"StartMessageDisable\": ");
			if (pos != line.npos) {
				startMessageDisableFound = true;
				startMessageDisable = line.find("true") == line.npos ? false : true;
			}
		}
		//read values of the "editable list" UI element
		pos = line.find("\"value\":");
		if (!line.empty() && pos != line.npos) {
			string temp = line.substr(pos + 10, line.npos - 1);
			stringstream lineStream = stringstream(temp);
			numValues = 0;
			//find Scene Round Trip
			if (temp.find("Scene Round Trip") != temp.npos) {
				//discard the first value ("Scene Round Trip")
				getline(lineStream, value, ',');
				while (lineStream.good()) {
					//Scene Round Trip,TriggerSceneHere,DelayHere,NextSceneHere,DelayHere,AnotherSceneHere,DelayHere,...
					getline(lineStream, value, ',');
					sceneRoundTrip.push_back(value);
				}
				//remove trailing /" of last value
				if (sceneRoundTrip.size() > 0) {
					sceneRoundTrip.back().pop_back();
				}
				//add missing values
				if (sceneRoundTrip.size() == 0 || sceneRoundTrip.size() % 2 != 0) {
					sceneRoundTrip.push_back("");
				}
			}
			//find Pause Scene Names
			if (temp.find("Pause Scene Names") != temp.npos) {
				//discard the first value ("Pause Scene Names")
				getline(lineStream, value, ',');
				while (lineStream.good()) {
					//Scene Round Trip,TriggerSceneHere,DelayHere,NextSceneHere,DelayHere,AnotherSceneHere,DelayHere,...
					getline(lineStream, value, ',');
					pauseScenes.push_back(value);
				}
				//remove trailing /" of last value
				if (pauseScenes.size() > 0) {
					pauseScenes.back().pop_back();
				}
			}
			//find window names to ignroe
			if (temp.find("Ignore Window Names") != temp.npos) {
				//discard the first value "Ignore Window Names"
				getline(lineStream, value, ',');
				while (lineStream.good()) {
					//Ignore Window Names,windowName1,windowName2,...
					getline(lineStream, value, ',');
					ignoreNames.push_back(value);
				}
				//remove trailing /" of last value
				if (ignoreNames.size() > 0) {
					ignoreNames.back().pop_back();
				}
			}
			//find values for Scene switching
			if (temp.find("Pause Scene Names") == temp.npos && temp.find("Scene Round Trip") == temp.npos && temp.find("Ignore Window Names") == temp.npos)
			{
				//windowTitle,sceneName,isFullscreenValue
				while (lineStream.good()) {
					getline(lineStream, value, ',');
					settingsElements.push_back(value);
					numValues++;
				}
				//remove trailing /" of last value
				if (settingsElements.size() > 0) {
					settingsElements.back().pop_back();
				}
				//two values per line are expected
				//add missing value
				for (; numValues < 3; numValues++) {
					settingsElements.push_back("");
				}
				//discard additional values
				for (; numValues > 3; numValues--) {
					settingsElements.pop_back();
				}

				//assing to data
				Data d = Data();
				string isFullscreen = settingsElements.back();
				d.isFullscreen = (isFullscreen.find("isFullscreen") != isFullscreen.npos) ? true : false;
				settingsElements.pop_back();
				string sceneName = settingsElements.back();
				d.sceneName = sceneName;
				settingsElements.pop_back();
				string windowName = settingsElements.back();
				settings.insert(pair<string, Data>(windowName, d));
				settingsElements.pop_back();
			}
		}
	}
	infile.close();
}

bool Settings::getStartMessageDisable() {
	return startMessageDisable;
}

map<string, Data> Settings::getMap() {
	return settings;
}

vector<string> Settings::getSceneRoundTrip() {
	return sceneRoundTrip;
}

vector<string> Settings::getPauseScenes() {
	return pauseScenes;
}

vector<string> Settings::getIgnoreNames()
{
	return ignoreNames;
}

