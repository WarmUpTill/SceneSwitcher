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
	//clear settings map
	settings = map<string, Data> ();
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
		//get switcher info
		pos = line.find("\"value\":");
		if (!line.empty() && pos != string::npos) {
			string temp = line.substr(pos + 10, string::npos - 1);
			temp.pop_back();
			stringstream lineStream = stringstream(temp);
			numValues = 0;
			while (lineStream.good()) {
				getline(lineStream, value, ',');
				settingsElements.push_back(value);
				numValues++;
			}
			//two values per line are expected
			//add missing value
			if(numValues < 3) { 
				settingsElements.push_back("");
			}
			//discard additional values
			for (numValues; numValues > 3; numValues--) { 
				settingsElements.pop_back();
			}

			//assing to data
			Data d = Data();
			string isFullscreen = settingsElements.back();
			d.isFullscreen = (isFullscreen.find("isFullscreen") == string::npos) ? false : true;
			settingsElements.pop_back();
			string sceneName = settingsElements.back();
			d.sceneName = sceneName;
			settingsElements.pop_back();
			string windowName = settingsElements.back();
			Settings::addToMap(windowName,d);
			settingsElements.pop_back();
		}
	}
	infile.close();
}

bool Settings::getStartMessageDisable()
{
	return startMessageDisable;
}

void Settings::addToMap(string s1, Data s2) {
	settings.insert(pair<string, Data>(s1, s2));
}

map<string, Data> Settings::getMap() {
	return settings;
}

