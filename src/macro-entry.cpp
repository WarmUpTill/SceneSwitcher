#include "headers/macro-entry.hpp"

#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"

std::unordered_map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
	{LogicType::NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::AND, {"AdvSceneSwitcher.logic.and"}},
	{LogicType::OR, {"AdvSceneSwitcher.logic.or"}},
	{LogicType::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{LogicType::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
	{LogicType::ROOT_NONE, {"AdvSceneSwitcher.logic.rootNone"}},
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

bool Macro::Save(obs_data_t *obj)
{
	obs_data_set_string(obj, "name", _name.c_str());

	obs_data_array_t *conditions = obs_data_array_create();
	for (auto &c : _conditions) {
		obs_data_t *array_obj = obs_data_create();

		c->Save(array_obj);
		obs_data_array_push_back(conditions, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "conditions", conditions);
	obs_data_array_release(conditions);

	obs_data_array_t *actions = obs_data_array_create();
	for (auto &a : _actions) {
		obs_data_t *array_obj = obs_data_create();

		//a->Save(array_obj);
		obs_data_array_push_back(actions, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "actions", actions);
	obs_data_array_release(actions);

	return true;
}

bool Macro::Load(obs_data_t *obj)
{
	return false;
}
