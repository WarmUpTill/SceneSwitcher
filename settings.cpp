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
				if (sceneRoundTrip.size() == 0 || sceneRoundTrip.size() % 2 != 0) {
					sceneRoundTrip.push_back("");
				}
			}
			//find values for Scene switching
			else
			{
				//windowTitle,sceneName,isFullscreenValue
				while (lineStream.good()) {
					getline(lineStream, value, ',');
					settingsElements.push_back(value);
					numValues++;
				}
				//two values per line are expected
				//add missing value
				if (numValues < 3) {
					settingsElements.push_back("");
				}
				//discard additional values
				for (numValues; numValues > 3; numValues--) {
					settingsElements.pop_back();
				}

				//assing to data
				Data d = Data();
				string isFullscreen = settingsElements.back();
				d.isFullscreen = (isFullscreen.find("isFullscreen") != isFullscreen.npos) ? true : false;
				//dont remove an element if isFullscreen was not found
				if (d.isFullscreen) {
					settingsElements.pop_back();
				}
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

bool Settings::getStartMessageDisable(){
	return startMessageDisable;
}

map<string, Data> Settings::getMap() {
	return settings;
}

vector<string> Settings::getSceneRoundTrip(){
	return sceneRoundTrip;
}

