#include "headers/macro.hpp"

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
	_matched = false;
	for (auto &c : _conditions) {
		bool cond = c->CheckCondition();

		switch (c->GetLogicType()) {
		case LogicType::NONE:
			blog(LOG_INFO, "ignoring condition check 'none'");
			continue;
			break;
		case LogicType::AND:
			_matched = _matched && cond;
			break;
		case LogicType::OR:
			_matched = _matched || cond;
			break;
		case LogicType::AND_NOT:
			_matched = _matched && !cond;
			break;
		case LogicType::OR_NOT:
			_matched = _matched || !cond;
			break;
		case LogicType::ROOT_NONE:
			_matched = cond;
			break;
		case LogicType::ROOT_NOT:
			_matched = !cond;
			break;
		default:
			blog(LOG_INFO, "ignoring unkown condition check");
			break;
		}
	}

	return _matched;
}

bool Macro::performAction()
{
	bool ret = true;
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

		a->Save(array_obj);
		obs_data_array_push_back(actions, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "actions", actions);
	obs_data_array_release(actions);

	return true;
}

bool Macro::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "name");

	obs_data_array_t *conditions = obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(conditions, i);

		int id = obs_data_get_int(array_obj, "id");

		_conditions.emplace_back(MacroConditionFactory::Create(id));
		_conditions.back()->Load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(conditions);

	obs_data_array_t *actions = obs_data_get_array(obj, "actions");
	count = obs_data_array_count(actions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(actions, i);

		int id = obs_data_get_int(array_obj, "id");

		_actions.emplace_back(MacroActionFactory::Create(id));
		_actions.back()->Load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(actions);

	return true;
}

bool MacroCondition::Save(obs_data_t *obj)
{
	obs_data_set_int(obj, "id", GetId());
	obs_data_set_int(obj, "logic", static_cast<int>(_logic));
	return true;
}

bool MacroCondition::Load(obs_data_t *obj)
{
	_logic = static_cast<LogicType>(obs_data_get_int(obj, "logic"));
	return true;
}

bool MacroAction::Save(obs_data_t *obj)
{
	obs_data_set_int(obj, "id", GetId());
	return true;
}

bool MacroAction::Load(obs_data_t *obj)
{
	return true;
}

void SwitcherData::saveMacros(obs_data_t *obj)
{
	obs_data_array_t *macroArray = obs_data_array_create();
	for (auto &m : macros) {
		obs_data_t *array_obj = obs_data_create();

		m.Save(array_obj);
		obs_data_array_push_back(macroArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "macros", macroArray);
	obs_data_array_release(macroArray);
}

void SwitcherData::loadMacros(obs_data_t *obj)
{
	macros.clear();

	obs_data_array_t *macroArray = obs_data_get_array(obj, "macros");
	size_t count = obs_data_array_count(macroArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(macroArray, i);
		macros.emplace_back();
		macros.back().Load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(macroArray);
}

bool SwitcherData::checkMacros()
{
	bool ret = false;
	for (auto &m : macros) {
		if (m.checkMatch()) {
			ret = true;
		}
	}
	return ret;
}

bool SwitcherData::runMacros()
{
	for (auto &m : macros) {
		if (m.Matched()) {
			if (!m.performAction()) {
				return false;
			}
		}
	}
	return true;
}
