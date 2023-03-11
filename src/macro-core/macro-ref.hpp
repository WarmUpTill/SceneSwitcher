#pragma once
#include <memory>
#include <string>
#include <vector>
#include <QString>
#include <obs-module.h>

class Macro;

class MacroRef {
public:
	MacroRef(){};
	MacroRef(std::string name);
	void operator=(const QString &);
	void operator=(const std::shared_ptr<Macro> &);
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);
	void PostLoad();
	std::shared_ptr<Macro> GetMacro() const;
	std::string Name() const;

private:
	std::string _postLoadName;
	std::weak_ptr<Macro> _macro;
};

void SaveMacroList(obs_data_t *obj, const std::vector<MacroRef> &macros,
		   const std::string &name = "macros");
void LoadMacroList(obs_data_t *obj, std::vector<MacroRef> &macros,
		   const std::string &name = "macros");
