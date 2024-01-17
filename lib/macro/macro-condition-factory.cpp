#include "macro-condition-factory.hpp"

namespace advss {

std::map<std::string, MacroConditionInfo> &MacroConditionFactory::GetMap()
{
	static std::map<std::string, MacroConditionInfo> _methods;
	return _methods;
}

bool MacroConditionFactory::Register(const std::string &id,
				     MacroConditionInfo info)
{
	if (auto it = GetMap().find(id); it == GetMap().end()) {
		GetMap()[id] = info;
		return true;
	}
	return false;
}

std::shared_ptr<MacroCondition>
MacroConditionFactory::Create(const std::string &id, Macro *m)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._create(m);
	}
	return nullptr;
}

QWidget *
MacroConditionFactory::CreateWidget(const std::string &id, QWidget *parent,
				    std::shared_ptr<MacroCondition> cond)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._createWidget(parent, cond);
	}
	return nullptr;
}

std::string MacroConditionFactory::GetConditionName(const std::string &id)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._name;
	}
	return "unknown condition";
}

std::string MacroConditionFactory::GetIdByName(const QString &name)
{
	for (auto it : GetMap()) {
		if (name == obs_module_text(it.second._name.c_str())) {
			return it.first;
		}
	}
	return "";
}

bool MacroConditionFactory::UsesDurationModifier(const std::string &id)
{
	if (auto it = GetMap().find(id); it != GetMap().end()) {
		return it->second._useDurationModifier;
	}
	return false;
}

} // namespace advss
