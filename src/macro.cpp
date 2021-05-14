#include "headers/macro.hpp"

#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/macro-action-switch-scene.hpp"

const std::map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
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

bool Macro::CeckMatch()
{
	_matched = false;
	for (auto &c : _conditions) {
		bool cond = c->CheckCondition();

		switch (c->GetLogicType()) {
		case LogicType::NONE:
			vblog(LOG_INFO,
			      "ignoring condition check 'none' for '%s'",
			      _name.c_str());
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
			blog(LOG_WARNING,
			     "ignoring unkown condition check for '%s'",
			     _name.c_str());
			break;
		}
	}

	return _matched;
}

bool Macro::PerformAction()
{
	bool ret = true;
	for (auto &a : _actions) {
		ret = ret && a->PerformAction();
		a->LogAction();
		if (!ret) {
			return false;
		}
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

bool isValidLogic(LogicType t, bool root)
{
	bool isRoot = isRootLogicType(t);
	if (!isRoot == root) {
		return false;
	}
	if (isRoot) {
		if (t >= LogicType::ROOT_LAST) {
			return false;
		}
	} else if (t >= LogicType::LAST) {
		return false;
	}
	return true;
}

void setValidLogic(MacroCondition *c, bool root, std::string name)
{
	if (isValidLogic(c->GetLogicType(), root)) {
		return;
	}
	if (root) {
		c->SetLogicType(LogicType::ROOT_NONE);
		blog(LOG_WARNING,
		     "setting invalid logic selection to 'if' for macro %s",
		     name.c_str());
	} else {
		c->SetLogicType(LogicType::NONE);
		blog(LOG_WARNING,
		     "setting invalid logic selection to 'ignore' for macro %s",
		     name.c_str());
	}
}

bool Macro::Load(obs_data_t *obj)
{
	_name = obs_data_get_string(obj, "name");
	bool root = true;

	obs_data_array_t *conditions = obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(conditions, i);

		int id = obs_data_get_int(array_obj, "id");

		auto newEntry = MacroConditionFactory::Create(id);
		if (newEntry) {
			_conditions.emplace_back(newEntry);
			auto c = _conditions.back().get();
			c->Load(array_obj);
			setValidLogic(c, root, _name);
		} else {
			blog(LOG_WARNING,
			     "discarding condition entry with unkown id (%d) for macro %s",
			     id, _name.c_str());
		}

		obs_data_release(array_obj);
		root = false;
	}
	obs_data_array_release(conditions);

	obs_data_array_t *actions = obs_data_get_array(obj, "actions");
	count = obs_data_array_count(actions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(actions, i);

		int id = obs_data_get_int(array_obj, "id");

		auto newEntry = MacroActionFactory::Create(id);
		if (newEntry) {
			_actions.emplace_back(newEntry);
			_actions.back()->Load(array_obj);
		} else {
			blog(LOG_WARNING,
			     "discarding action entry with unkown id (%d) for macro %s",
			     id, _name.c_str());
		}

		obs_data_release(array_obj);
	}
	obs_data_array_release(actions);

	return true;
}

bool Macro::SwitchesScene()
{
	MacroActionSwitchScene temp;
	for (auto &a : _actions) {
		if (a->GetId() == temp.GetId()) {
			return true;
		}
	}
	return false;
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
	UNUSED_PARAMETER(obj);
	return true;
}

void MacroAction::LogAction()
{
	blog(LOG_INFO, "performed action %d", GetId());
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
		if (m.CeckMatch()) {
			ret = true;
			// This has to be performed here for now as actions are
			// not performed immediately after checking conditions.
			if (m.SwitchesScene()) {
				switcher->macroSceneSwitched = true;
			}
		}
	}
	return ret;
}

bool SwitcherData::runMacros()
{
	for (auto &m : macros) {
		if (m.Matched()) {
			blog(LOG_INFO, "running macro: %s", m.Name().c_str());
			if (!m.PerformAction()) {
				blog(LOG_WARNING, "abort macro: %s",
				     m.Name().c_str());
				return false;
			}
		}
	}
	return true;
}
