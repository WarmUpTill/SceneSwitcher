#pragma once
#include "export-symbol-helper.hpp"

#include <memory>
#include <string>
#include <vector>
#include <QString>
#include <obs-data.h>

namespace advss {

class Macro;

class MacroRef {
public:
	MacroRef(){};
	EXPORT MacroRef(std::string name);
	EXPORT void operator=(const QString &);
	EXPORT void operator=(const std::shared_ptr<Macro> &);
	EXPORT void Save(obs_data_t *obj) const;
	EXPORT void Load(obs_data_t *obj);
	void PostLoad();
	EXPORT std::shared_ptr<Macro> GetMacro() const;
	EXPORT std::string Name() const;

private:
	std::string _postLoadName;
	std::weak_ptr<Macro> _macro;
};

EXPORT void SaveMacroList(obs_data_t *obj, const std::vector<MacroRef> &macros,
			  const std::string &name = "macros");
EXPORT void LoadMacroList(obs_data_t *obj, std::vector<MacroRef> &macros,
			  const std::string &name = "macros");

} // namespace advss
