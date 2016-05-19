#pragma once
#include <map>


#ifndef SETTINGS_H
#define SETTINGS_H

using namespace std;
class Settings {
	map<string, string> settings;
public:
	void load(string);
	map<string, string> getMap();
private:
	void addToMap(string, string);

};

#endif