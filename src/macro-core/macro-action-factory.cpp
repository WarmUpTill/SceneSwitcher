#include "macro-action-factory.hpp"

namespace advss {

std::map<std::string, MacroActionInfo> &MacroActionFactory::GetMap()
{
	static std::map<std::string, MacroActionInfo> _methods;
	return _methods;
}

bool MacroActionFactory::Register(const std::string &id, MacroActionInfo info)
{
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroAction> MacroActionFactory::Create(const std::string &id,
							Macro *m)
{
	if (auto it = GetMap().find(id); it != GetMap().end())
		return it->second._create(m);

	return nullptr;
}

QWidget *MacroActionFactory::CreateWidget(const std::string &id,
					  QWidget *parent,
					  std::shared_ptr<MacroAction> action)
{
	if (auto it = GetMap().find(id); it != GetMap().end())
		return it->second._createWidget(parent, action);

	return nullptr;
}

std::string MacroActionFactory::GetActionName(const std::string &id)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return "unknown action";
}

std::string MacroActionFactory::GetIdByName(const QString &name)
{
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

} // namespace advss
