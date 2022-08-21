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
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);
	Macro *get();
	Macro *operator->();

private:
	std::string _name = "";
	Macro *_ref = nullptr;
};

void SaveMacroList(obs_data_t *obj, const std::vector<MacroRef> &macros,
		   const std::string &name = "macros");
void LoadMacroList(obs_data_t *obj, std::vector<MacroRef> &macros,
		   const std::string &name = "macros");
