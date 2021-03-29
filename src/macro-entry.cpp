#include "headers/macro-entry.hpp"

#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"

std::unordered_map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
	{LogicType::NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::AND, {"AdvSceneSwitcher.logic.and"}},
	{LogicType::OR, {"AdvSceneSwitcher.logic.or"}},
	{LogicType::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{LogicType::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
	{LogicType::ROOT_NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::ROOT_NOT, {"AdvSceneSwitcher.logic.not"}},
};

Macro::Macro(std::string name) : _name(name) {}

Macro::~Macro() {}

bool Macro::checkMatch()
{
	bool match = false;
	for (auto &c : _conditions) {
		bool cond = c->CheckCondition();

		switch (c->GetLogicType()) {
		case LogicType::NONE:
			blog(LOG_INFO, "ignoring condition check 'none'");
			continue;
			break;
		case LogicType::AND:
			match = match && cond;
			break;
		case LogicType::OR:
			match = match || cond;
			break;
		case LogicType::AND_NOT:
			match = match && !cond;
			break;
		case LogicType::OR_NOT:
			match = match || !cond;
			break;
		case LogicType::ROOT_NONE:
			match = cond;
			break;
		case LogicType::ROOT_NOT:
			match = !cond;
			break;
		default:
			blog(LOG_INFO, "ignoring unkown condition check");
			break;
		}
	}

	return match;
}

bool Macro::performAction()
{
	bool ret = false;
	for (auto &a : _actions) {
		ret = ret && a->PerformAction();
	}

	return ret;
}
