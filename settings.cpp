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
	//read the settings file
	std::vector<std::string> settingsElements;
	int numValues = 0;
	ifstream infile(settingsFilePath);
	string value;
	string line;
	while (infile.good())
	{
		//read json file
		getline(infile, line);
		size_t pos = line.find("\"value\":");
		if (!line.empty() && pos != -1) {
			string temp = line.substr(pos + 10, string::npos - 1);
			temp.pop_back();
			stringstream lineStream = stringstream(temp);
			while (lineStream.good()) {
				getline(lineStream, value, ',');
				settingsElements.push_back(value);
				numValues++;
			}
		}
	}
	//create settings map containing windowname and desired scene
	for (int i = 0; i < numValues; ) {
		string s2 = settingsElements.back();
		settingsElements.pop_back();
		i++;
		string s1 = settingsElements.back();
		settingsElements.pop_back();
		i++;
		//window name,scene
		Settings::addToMap(s1, s2);
	}
}

void Settings::addToMap(string s1, string s2) {
	settings.insert(pair<string, string>(s1, s2));
}

map<string, string> Settings::getMap() {
	return settings;
}

