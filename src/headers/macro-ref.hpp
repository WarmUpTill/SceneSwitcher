#pragma once
#include <string>
#include <QString>
#include <obs-module.h>

class Macro;

class MacroRef {
public:
	MacroRef(){};
	MacroRef(std::string name);
	void UpdateRef();
	void UpdateRef(std::string name);
	void UpdateRef(QString name);
	void Save(obs_data_t *obj);
	void Load(obs_data_t *obj);
	Macro *get();
	Macro *operator->();

private:
	std::string _name = "";
	Macro *_ref = nullptr;
};
