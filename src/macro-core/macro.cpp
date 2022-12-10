#include "macro.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "macro-action-scene-switch.hpp"
#include "advanced-scene-switcher.hpp"

#include <limits>
#undef max
#include <chrono>
#include <unordered_map>

constexpr int perfLogThreshold = 300;

Macro::Macro(const std::string &name, const bool addHotkey)
{
	SetName(name);
	if (addHotkey) {
		SetupHotkeys();
	}
	_registerHotkeys = addHotkey;
}

Macro::~Macro()
{
	_die = true;
	Stop();
	ClearHotkeys();
}

bool Macro::CeckMatch()
{
	_matched = false;
	for (auto &c : _conditions) {
		if (_paused) {
			vblog(LOG_INFO, "Macro %s is paused", _name.c_str());
			return false;
		}

		auto startTime = std::chrono::high_resolution_clock::now();
		bool cond = c->CheckCondition();
		auto endTime = std::chrono::high_resolution_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
			endTime - startTime);
		if (ms.count() >= perfLogThreshold) {
			blog(LOG_WARNING,
			     "spent %ld ms in %s condition check of macro '%s'!",
			     ms.count(), c->GetId().c_str(), Name().c_str());
		}

		c->CheckDurationModifier(cond);

		switch (c->GetLogicType()) {
		case LogicType::NONE:
			vblog(LOG_INFO,
			      "ignoring condition check 'none' for '%s'",
			      _name.c_str());
			continue;
		case LogicType::AND:
			_matched = _matched && cond;
			if (cond) {
				c->SetHighlight();
			}
			break;
		case LogicType::OR:
			_matched = _matched || cond;
			if (cond) {
				c->SetHighlight();
			}
			break;
		case LogicType::AND_NOT:
			_matched = _matched && !cond;
			if (!cond) {
				c->SetHighlight();
			}
			break;
		case LogicType::OR_NOT:
			if (!cond) {
				c->SetHighlight();
			}
			break;
		case LogicType::ROOT_NONE:
			_matched = cond;
			if (cond) {
				c->SetHighlight();
			}
			break;
		case LogicType::ROOT_NOT:
			_matched = !cond;
			if (!cond) {
				c->SetHighlight();
			}
			break;
		default:
			blog(LOG_WARNING,
			     "ignoring unknown condition check for '%s'",
			     _name.c_str());
			break;
		}
		vblog(LOG_INFO, "condition %s returned %d", c->GetId().c_str(),
		      cond);
	}
	vblog(LOG_INFO, "Macro %s returned %d", _name.c_str(), _matched);

	bool newLastMatched = _matched;
	if (_matched && _matchOnChange && _lastMatched == _matched) {
		vblog(LOG_INFO, "ignore match for Macro %s (on change)",
		      _name.c_str());
		_matched = false;
		SetOnChangeHighlight();
	}
	_lastMatched = newLastMatched;

	// TODO: Move back to PerformAction() once new scene collection frontend
	// events are available - see:
	// https://github.com/obsproject/obs-studio/commit/feda1aaa283e8a99f6ba1159cfe6b9c1f2934a61
	if (_matched && _count != std::numeric_limits<int>::max()) {
		_count++;
	}
	_lastCheckTime = std::chrono::high_resolution_clock::now();
	return _matched;
}

bool Macro::PerformActions(bool forceParallel, bool ignorePause)
{
	if (!_done) {
		vblog(LOG_INFO, "macro %s already running", _name.c_str());
		return !forceParallel;
	}
	_stop = false;
	_done = false;
	bool ret = true;
	if (_runInParallel || forceParallel) {
		if (_backgroundThread.joinable()) {
			_backgroundThread.join();
		}
		_backgroundThread = std::thread(
			[this, ignorePause] { RunActions(ignorePause); });
	} else {
		RunActions(ret, ignorePause);
	}
	_wasExecutedRecently = true;
	return ret;
}

int64_t Macro::MsSinceLastCheck()
{
	if (_lastCheckTime.time_since_epoch().count() == 0) {
		return 0;
	}
	const auto timePassed =
		std::chrono::high_resolution_clock::now() - _lastCheckTime;
	return std::chrono::duration_cast<std::chrono::milliseconds>(timePassed)
		       .count() +
	       1;
}

void Macro::SetName(const std::string &name)
{
	_name = name;
	SetHotkeysDesc();
}

void Macro::ResetTimers()
{
	for (auto &c : _conditions) {
		c->ResetDuration();
	}
	_lastCheckTime = {};
}

void Macro::RunActions(bool &retVal, bool ignorePause)
{
	bool ret = true;
	for (auto &a : _actions) {
		a->LogAction();
		ret = ret && a->PerformAction();
		if (!ret || (_paused && !ignorePause) || _stop || _die) {
			retVal = ret;
			break;
		}
		a->SetHighlight();
	}
	_done = true;
}

void Macro::RunActions(bool ignorePause)
{
	bool unused;
	RunActions(unused, ignorePause);
}

void Macro::SetOnChangeHighlight()
{
	_onChangeTriggered = true;
}

void Macro::SetPaused(bool pause)
{
	if (_paused && !pause) {
		ResetTimers();
	}
	_paused = pause;
}

void Macro::AddHelperThread(std::thread &&newThread)
{
	for (unsigned int i = 0; i < _helperThreads.size(); i++) {
		if (!_helperThreads[i].joinable()) {
			_helperThreads[i] = std::move(newThread);
			return;
		}
	}
	_helperThreads.push_back(std::move(newThread));
}

void Macro::Stop()
{
	_stop = true;
	switcher->macroWaitCv.notify_all();
	for (auto &t : _helperThreads) {
		if (t.joinable()) {
			t.join();
		}
	}
	if (_backgroundThread.joinable()) {
		_backgroundThread.join();
	}
}

void Macro::UpdateActionIndices()
{
	int idx = 0;
	for (auto a : _actions) {
		a->SetIndex(idx);
		idx++;
	}
}

void Macro::UpdateConditionIndices()
{
	int idx = 0;
	for (auto c : _conditions) {
		c->SetIndex(idx);
		idx++;
	}
}

bool Macro::Save(obs_data_t *obj)
{
	obs_data_set_string(obj, "name", _name.c_str());
	obs_data_set_bool(obj, "pause", _paused);
	obs_data_set_bool(obj, "parallel", _runInParallel);
	obs_data_set_bool(obj, "onChange", _matchOnChange);

	obs_data_set_bool(obj, "registerHotkeys", _registerHotkeys);
	obs_data_array_t *pauseHotkey = obs_hotkey_save(_pauseHotkey);
	obs_data_set_array(obj, "pauseHotkey", pauseHotkey);
	obs_data_array_release(pauseHotkey);
	obs_data_array_t *unpauseHotkey = obs_hotkey_save(_unpauseHotkey);
	obs_data_set_array(obj, "unpauseHotkey", unpauseHotkey);
	obs_data_array_release(unpauseHotkey);
	obs_data_array_t *togglePauseHotkey =
		obs_hotkey_save(_togglePauseHotkey);
	obs_data_set_array(obj, "togglePauseHotkey", togglePauseHotkey);
	obs_data_array_release(togglePauseHotkey);

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
	_runInParallel = obs_data_get_bool(obj, "parallel");
	_matchOnChange = obs_data_get_bool(obj, "onChange");

	obs_data_set_default_bool(obj, "registerHotkeys", true);
	_registerHotkeys = obs_data_get_bool(obj, "registerHotkeys");
	if (_registerHotkeys) {
		SetupHotkeys();
	}
	obs_data_array_t *pauseHotkey = obs_data_get_array(obj, "pauseHotkey");
	obs_hotkey_load(_pauseHotkey, pauseHotkey);
	obs_data_array_release(pauseHotkey);
	obs_data_array_t *unpauseHotkey =
		obs_data_get_array(obj, "unpauseHotkey");
	obs_hotkey_load(_unpauseHotkey, unpauseHotkey);
	obs_data_array_release(unpauseHotkey);
	obs_data_array_t *togglePauseHotkey =
		obs_data_get_array(obj, "togglePauseHotkey");
	obs_hotkey_load(_togglePauseHotkey, togglePauseHotkey);
	obs_data_array_release(togglePauseHotkey);
	SetHotkeysDesc();

	bool root = true;
	obs_data_array_t *conditions = obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(conditions, i);

		std::string id = obs_data_get_string(array_obj, "id");

		auto newEntry = MacroConditionFactory::Create(id, this);
		if (newEntry) {
			_conditions.emplace_back(newEntry);
			auto c = _conditions.back().get();
			c->Load(array_obj);
			setValidLogic(c, root, _name);
		} else {
			blog(LOG_WARNING,
			     "discarding condition entry with unknown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}

		obs_data_release(array_obj);
		root = false;
	}
	obs_data_array_release(conditions);
	UpdateConditionIndices();

	obs_data_array_t *actions = obs_data_get_array(obj, "actions");
	count = obs_data_array_count(actions);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(actions, i);

		std::string id = obs_data_get_string(array_obj, "id");

		auto newEntry = MacroActionFactory::Create(id, this);
		if (newEntry) {
			_actions.emplace_back(newEntry);
			_actions.back()->Load(array_obj);
		} else {
			blog(LOG_WARNING,
			     "discarding action entry with unknown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}

		obs_data_release(array_obj);
	}
	obs_data_array_release(actions);
	UpdateActionIndices();
	return true;
}

void Macro::ResolveMacroRef()
{
	for (auto &c : _conditions) {
		MacroRefCondition *ref =
			dynamic_cast<MacroRefCondition *>(c.get());
		if (ref) {
			ref->ResolveMacroRef();
		}
		MultiMacroRefCondtition *ref2 =
			dynamic_cast<MultiMacroRefCondtition *>(c.get());
		if (ref2) {
			ref2->ResolveMacroRef();
		}
	}
	for (auto &a : _actions) {
		MacroRefAction *ref = dynamic_cast<MacroRefAction *>(a.get());
		if (ref) {
			ref->ResolveMacroRef();
		}
		MultiMacroRefAction *ref2 =
			dynamic_cast<MultiMacroRefAction *>(a.get());
		if (ref2) {
			ref2->ResolveMacroRef();
		}
	}
}

bool Macro::SwitchesScene()
{
	MacroActionSwitchScene temp(nullptr);
	auto sceneSwitchId = temp.GetId();
	for (auto &a : _actions) {
		if (a->GetId() == sceneSwitchId) {
			return true;
		}
	}
	return false;
}

bool Macro::WasExecutedRecently()
{
	if (_wasExecutedRecently) {
		_wasExecutedRecently = false;
		return true;
	}
	return false;
}

bool Macro::OnChangePreventedActionsRecently()
{
	if (_onChangeTriggered) {
		_onChangeTriggered = false;
		return true;
	}
	return false;
}

void Macro::ResetUIHelpers()
{
	_onChangeTriggered = false;
	for (auto c : _conditions) {
		c->Highlight();
	}
	for (auto a : _actions) {
		a->Highlight();
	}
}

void Macro::EnablePauseHotkeys(bool value)
{
	if (_registerHotkeys == value) {
		return;
	}

	if (_registerHotkeys) {
		ClearHotkeys();
	} else {
		SetupHotkeys();
	}
	_registerHotkeys = value;
}

bool Macro::PauseHotkeysEnabled()
{
	return _registerHotkeys;
}

static void pauseCB(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		auto m = static_cast<Macro *>(data);
		m->SetPaused(true);
	}
}

static void unpauseCB(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		auto m = static_cast<Macro *>(data);
		m->SetPaused(false);
	}
}

static void togglePauseCB(void *data, obs_hotkey_id, obs_hotkey_t *,
			  bool pressed)
{
	if (pressed) {
		auto m = static_cast<Macro *>(data);
		m->SetPaused(!m->Paused());
	}
}

static int macroHotkeyID = 0;

obs_hotkey_id registerHotkeyHelper(const std::string prefix,
				   const char *formatModuleText, Macro *macro,
				   obs_hotkey_func func)
{
	macroHotkeyID++;

	std::string hotkeyName = prefix + std::to_string(macroHotkeyID);
	QString format{obs_module_text(formatModuleText)};
	QString hotkeyDesc = format.arg(QString::fromStdString(macro->Name()));
	return obs_hotkey_register_frontend(hotkeyName.c_str(),
					    hotkeyDesc.toStdString().c_str(),
					    func, macro);
}

void Macro::SetupHotkeys()
{
	if (_pauseHotkey != OBS_INVALID_HOTKEY_ID ||
	    _unpauseHotkey != OBS_INVALID_HOTKEY_ID ||
	    _togglePauseHotkey != OBS_INVALID_HOTKEY_ID) {
		ClearHotkeys();
	}

	_pauseHotkey = registerHotkeyHelper(
		"macro_pause_hotkey_", "AdvSceneSwitcher.hotkey.macro.pause",
		this, pauseCB);
	_unpauseHotkey = registerHotkeyHelper(
		"macro_unpause_hotkey_",
		"AdvSceneSwitcher.hotkey.macro.unpause", this, unpauseCB);
	_togglePauseHotkey = registerHotkeyHelper(
		"macro_toggle_pause_hotkey_",
		"AdvSceneSwitcher.hotkey.macro.togglePause", this,
		togglePauseCB);
}

void Macro::ClearHotkeys()
{
	obs_hotkey_unregister(_pauseHotkey);
	obs_hotkey_unregister(_unpauseHotkey);
	obs_hotkey_unregister(_togglePauseHotkey);
}

void setHotkeyDescriptionHelper(const char *formatModuleText,
				const std::string name, const obs_hotkey_id id)
{
	QString format{obs_module_text(formatModuleText)};
	QString hotkeyDesc = format.arg(QString::fromStdString(name));
	obs_hotkey_set_description(id, hotkeyDesc.toStdString().c_str());
}

void Macro::SetHotkeysDesc()
{
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.pause", _name,
				   _pauseHotkey);
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.unpause",
				   _name, _unpauseHotkey);
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.togglePause",
				   _name, _togglePauseHotkey);
}

void SwitcherData::saveMacros(obs_data_t *obj)
{
	switcher->macroProperties.Save(obj);

	obs_data_array_t *macroArray = obs_data_array_create();
	for (auto &m : macros) {
		obs_data_t *array_obj = obs_data_create();

		m->Save(array_obj);
		obs_data_array_push_back(macroArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "macros", macroArray);
	obs_data_array_release(macroArray);
}

void SwitcherData::loadMacros(obs_data_t *obj)
{
	Hotkey::ClearAllHotkeys();
	switcher->macroProperties.Load(obj);

	macros.clear();
	obs_data_array_t *macroArray = obs_data_get_array(obj, "macros");
	size_t count = obs_data_array_count(macroArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(macroArray, i);
		macros.emplace_back(std::make_shared<Macro>());
		macros.back()->Load(array_obj);
		obs_data_release(array_obj);
	}
	obs_data_array_release(macroArray);

	for (auto &m : macros) {
		m->ResolveMacroRef();
	}
}

bool SwitcherData::checkMacros()
{
	bool ret = false;
	for (auto &m : macros) {
		if (m->CeckMatch()) {
			ret = true;
			// This has to be performed here for now as actions are
			// not performed immediately after checking conditions.
			if (m->SwitchesScene()) {
				switcher->macroSceneSwitched = true;
			}
		}
	}
	return ret;
}

bool SwitcherData::runMacros()
{
	// TODO: Don't rely on creating a copy of each macro once new frontend
	// events are available - see:
	// https://github.com/obsproject/obs-studio/commit/feda1aaa283e8a99f6ba1159cfe6b9c1f2934a61
	for (auto m : macros) {
		if (m->Matched()) {
			vblog(LOG_INFO, "running macro: %s", m->Name().c_str());
			if (!m->PerformActions()) {
				blog(LOG_WARNING, "abort macro: %s",
				     m->Name().c_str());
			}
		}
	}
	return true;
}

Macro *GetMacroByName(const char *name)
{
	for (auto &m : switcher->macros) {
		if (m->Name() == name) {
			return m.get();
		}
	}

	return nullptr;
}

Macro *GetMacroByQString(const QString &name)
{
	return GetMacroByName(name.toUtf8().constData());
}
