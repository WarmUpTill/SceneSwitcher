#include "headers/macro.hpp"

#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/macro-action-scene-switch.hpp"

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
		vblog(LOG_INFO, "condition %s returned %d", c->GetId().c_str(),
		      cond);
	}

	vblog(LOG_INFO, "Macro %s returned %d", _name.c_str(), _matched);

	// Condition checks shall still be run even if macro is paused.
	// Otherwise conditions might behave in unexpected ways when resuming.
	//
	// For example, audio could immediately match after unpause, when it
	// matched before it was paused due to timers not being updated.
	if (_paused) {
		vblog(LOG_INFO, "Macro %s is paused", _name.c_str());
		_matched = false;
		return false;
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
	obs_data_set_bool(obj, "pause", _paused);

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
	_paused = obs_data_get_bool(obj, "pause");
	bool root = true;

	obs_data_array_t *conditions = obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(conditions, i);

		std::string id = obs_data_get_string(array_obj, "id");

		auto newEntry = MacroConditionFactory::Create(id);
		if (newEntry) {
			_conditions.emplace_back(newEntry);
			auto c = _conditions.back().get();
			c->Load(array_obj);
			setValidLogic(c, root, _name);
		} else {
			blog(LOG_WARNING,
			     "discarding condition entry with unkown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}

		obs_data_release(array_obj);
		root = false;
	}
	obs_data_array_release(conditions);

	obs_data_array_t *actions = obs_data_get_array(obj, "actions");
	count = obs_data_array_count(actions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(actions, i);

		std::string id = obs_data_get_string(array_obj, "id");

		auto newEntry = MacroActionFactory::Create(id);
		if (newEntry) {
			_actions.emplace_back(newEntry);
			_actions.back()->Load(array_obj);
		} else {
			blog(LOG_WARNING,
			     "discarding action entry with unkown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}

		obs_data_release(array_obj);
	}
	obs_data_array_release(actions);

	return true;
}

bool Macro::SwitchesScene()
{
	MacroActionSwitchScene temp;
	auto sceneSwitchId = temp.GetId();
	for (auto &a : _actions) {
		if (a->GetId() == sceneSwitchId) {
			return true;
		}
	}
	return false;
}

bool MacroCondition::Save(obs_data_t *obj)
{
	obs_data_set_string(obj, "id", GetId().c_str());
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
	obs_data_set_string(obj, "id", GetId().c_str());
	return true;
}

bool MacroAction::Load(obs_data_t *obj)
{
	UNUSED_PARAMETER(obj);
	return true;
}

void MacroAction::LogAction()
{
	vblog(LOG_INFO, "performed action %s", GetId().c_str());
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

// Temporary helper functions to convert old settings format to new one
static std::unordered_map<int, std::string> actionIntToActionString = {
	{2, "audio"},     {4, "recording"},    {5, "replay_buffer"}, {6, "run"},
	{3, "streaming"}, {0, "scene_switch"}, {1, "wait"},
};

static void replaceActionIds(obs_data_t *obj)
{
	obs_data_array_t *actions = obs_data_get_array(obj, "actions");
	size_t count = obs_data_array_count(actions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(actions, i);
		auto oldId = obs_data_get_int(array_obj, "id");
		obs_data_set_string(array_obj, "id",
				    actionIntToActionString[oldId].c_str());
		obs_data_release(array_obj);
	}
	obs_data_array_release(actions);
}

static std::unordered_map<int, std::string> conditionIntToConditionString = {
	{3, "audio"},         {4, "file"},      {10, "idle"},     {5, "media"},
	{11, "plugin_state"}, {9, "process"},   {8, "recording"}, {2, "region"},
	{0, "scene"},         {7, "streaming"}, {6, "video"},     {1, "window"},
};

static void replaceConditionIds(obs_data_t *obj)
{
	obs_data_array_t *conditions = obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(conditions, i);
		auto oldId = obs_data_get_int(array_obj, "id");
		obs_data_set_string(
			array_obj, "id",
			conditionIntToConditionString[oldId].c_str());
		obs_data_release(array_obj);
	}
	obs_data_array_release(conditions);
}

static void convertOldMacroIdsToString(obs_data_t *obj)
{
	obs_data_array_t *macroArray = obs_data_get_array(obj, "macros");
	size_t count = obs_data_array_count(macroArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(macroArray, i);
		replaceActionIds(array_obj);
		replaceConditionIds(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(macroArray);
}

void SwitcherData::loadMacros(obs_data_t *obj)
{
	// TODO: Remove conversion helper in future version
	std::string previousVersion = obs_data_get_string(obj, "version");
	if (previousVersion == "2ce0b35921be892c987c7dbb5fc90db38f15f0a6") {
		convertOldMacroIdsToString(obj);
	}

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
			vblog(LOG_INFO, "running macro: %s", m.Name().c_str());
			if (!m.PerformAction()) {
				blog(LOG_WARNING, "abort macro: %s",
				     m.Name().c_str());
				return false;
			}
		}
	}
	return true;
}

Macro *GetMacroByName(const char *name)
{
	for (auto &m : switcher->macros) {
		if (m.Name() == name) {
			return &m;
		}
	}

	return nullptr;
}

Macro *GetMacroByQString(const QString &name)
{
	return GetMacroByName(name.toUtf8().constData());
}
