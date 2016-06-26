#pragma once

#include "stdafx.h"
#include <map>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <tchar.h>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include "shlobj.h"
#include "settings.h"


using namespace std;

string Settings::getSettingsFilePath()
{
	return settingsFilePath;
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
		getline(infile, line);
		if (!line.empty()) {
			stringstream lineStream = stringstream(line);
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

