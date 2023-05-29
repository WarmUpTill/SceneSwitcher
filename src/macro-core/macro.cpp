#include "macro.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "macro-dock.hpp"
#include "macro-action-scene-switch.hpp"
#include "switcher-data.hpp"
#include "hotkey.hpp"

#include <limits>
#undef max
#include <chrono>
#include <unordered_map>
#include <QMainWindow>

namespace advss {

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

	// Keep the dock widgets in case of shutdown so they can be rostored by
	// OBS on startup
	if (!switcher->obsIsShuttingDown) {
		RemoveDock();
	}
}

std::shared_ptr<Macro>
Macro::CreateGroup(const std::string &name,
		   std::vector<std::shared_ptr<Macro>> &children)
{
	auto group = std::make_shared<Macro>(name, false);
	for (auto &c : children) {
		c->SetParent(group);
	}
	group->_isGroup = true;
	group->_groupSize = children.size();
	return group;
}

void Macro::RemoveGroup(std::shared_ptr<Macro> group)
{
	auto it = std::find(switcher->macros.begin(), switcher->macros.end(),
			    group);
	if (it == switcher->macros.end()) {
		return;
	}

	auto size = group->GroupSize();
	for (uint32_t i = 1; i <= size; i++) {
		auto m = std::next(it, i);
		(*m)->SetParent(nullptr);
	}

	switcher->macros.erase(it);
}

void Macro::PrepareMoveToGroup(Macro *group, std::shared_ptr<Macro> item)
{
	for (auto &m : switcher->macros) {
		if (m.get() == group) {
			PrepareMoveToGroup(m, item);
			return;
		}
	}
	PrepareMoveToGroup(std::shared_ptr<Macro>(), item);
}

void Macro::PrepareMoveToGroup(std::shared_ptr<Macro> group,
			       std::shared_ptr<Macro> item)
{
	if (!item) {
		return;
	}

	// Potentially remove from old group
	auto oldGroup = item->Parent();
	if (oldGroup) {
		oldGroup->_groupSize--;
	}

	item->SetParent(group);
	if (group) {
		group->_groupSize++;
	}
}

bool Macro::CeckMatch()
{
	if (_isGroup) {
		return false;
	}

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
	if (_matched && _runCount != std::numeric_limits<int>::max()) {
		_runCount++;
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
	auto group = _parent.lock();
	if (group) {
		group->_wasExecutedRecently = true;
	}
	return ret;
}

int64_t Macro::MsSinceLastCheck() const
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
	SetDockWidgetName();
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

bool Macro::DockIsVisible() const
{
	return _dock && _dockAction && _dock->isVisible();
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

std::deque<std::shared_ptr<MacroCondition>> &Macro::Conditions()
{
	return _conditions;
}

std::deque<std::shared_ptr<MacroAction>> &Macro::Actions()
{
	return _actions;
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

std::shared_ptr<Macro> Macro::Parent() const
{
	return _parent.lock();
}

bool Macro::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "name", _name.c_str());
	obs_data_set_bool(obj, "pause", _paused);
	obs_data_set_bool(obj, "parallel", _runInParallel);
	obs_data_set_bool(obj, "onChange", _matchOnChange);

	obs_data_set_bool(obj, "group", _isGroup);
	if (_isGroup) {
		auto groupData = obs_data_create();
		obs_data_set_bool(groupData, "collapsed", _isCollapsed);
		obs_data_set_int(groupData, "size", _groupSize);
		obs_data_set_obj(obj, "groupData", groupData);
		obs_data_release(groupData);
		return true;
	}

	SaveDockSettings(obj);

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

	_isGroup = obs_data_get_bool(obj, "group");
	if (_isGroup) {
		auto groupData = obs_data_get_obj(obj, "groupData");
		_isCollapsed = obs_data_get_bool(groupData, "collapsed");
		_groupSize = obs_data_get_int(groupData, "size");
		obs_data_release(groupData);
		return true;
	}

	LoadDockSettings(obj);

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

bool Macro::PostLoad()
{
	for (auto &c : _conditions) {
		c->PostLoad();
	}
	for (auto &a : _actions) {
		a->PostLoad();
	}
	return true;
}

bool Macro::SwitchesScene() const
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

bool Macro::PauseHotkeysEnabled() const
{
	return _registerHotkeys;
}

void Macro::SaveDockSettings(obs_data_t *obj) const
{
	auto dockSettings = obs_data_create();
	obs_data_set_bool(dockSettings, "register", _registerDock);
	// The object name is used to restore the position of the dock
	if (_registerDock) {
		SetDockWidgetName();
	}
	obs_data_set_bool(dockSettings, "hasRunButton", _dockHasRunButton);
	obs_data_set_bool(dockSettings, "hasPauseButton", _dockHasPauseButton);
	obs_data_set_string(dockSettings, "runButtonText",
			    _runButtonText.c_str());
	obs_data_set_string(dockSettings, "pauseButtonText",
			    _pauseButtonText.c_str());
	obs_data_set_string(dockSettings, "unpauseButtonText",
			    _unpauseButtonText.c_str());
	if (_dock) {
		auto window = static_cast<QMainWindow *>(
			obs_frontend_get_main_window());
		obs_data_set_bool(dockSettings, "isFloating",
				  _dock->isFloating());
		obs_data_set_bool(dockSettings, "isVisible", DockIsVisible());
		obs_data_set_int(dockSettings, "area",
				 window->dockWidgetArea(_dock));
		obs_data_set_string(
			dockSettings, "geometry",
			_dock->saveGeometry().toBase64().constData());
	}
	obs_data_set_obj(obj, "dockSettings", dockSettings);
	obs_data_release(dockSettings);
}

void Macro::LoadDockSettings(obs_data_t *obj)
{
	auto dockSettings = obs_data_get_obj(obj, "dockSettings");
	if (!dockSettings) {
		// TODO: Remove this fallback
		_dockHasRunButton = obs_data_get_bool(obj, "dockHasRunButton");
		_dockHasPauseButton =
			obs_data_get_bool(obj, "dockHasPauseButton");
		EnableDock(obs_data_get_bool(obj, "registerDock"));
		return;
	}

	const bool dockEnabled = obs_data_get_bool(dockSettings, "register");
	_dockIsVisible = obs_data_get_bool(dockSettings, "isVisible");

	// TODO: remove these default settings in a future version
	obs_data_set_default_string(
		dockSettings, "runButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.run"));
	obs_data_set_default_string(
		dockSettings, "pauseButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.pause"));
	obs_data_set_default_string(
		dockSettings, "unpauseButtonText",
		obs_module_text("AdvSceneSwitcher.macroDock.unpause"));

	_runButtonText = obs_data_get_string(dockSettings, "runButtonText");
	_pauseButtonText = obs_data_get_string(dockSettings, "pauseButtonText");
	_unpauseButtonText =
		obs_data_get_string(dockSettings, "unpauseButtonText");
	if (dockEnabled) {
		_dockHasRunButton =
			obs_data_get_bool(dockSettings, "hasRunButton");
		_dockHasPauseButton =
			obs_data_get_bool(dockSettings, "hasPauseButton");
		_dockIsFloating = obs_data_get_bool(dockSettings, "isFloating");
		_dockArea = static_cast<Qt::DockWidgetArea>(
			obs_data_get_int(dockSettings, "area"));
		auto geometryStr =
			obs_data_get_string(dockSettings, "geometry");
		if (geometryStr && strlen(geometryStr)) {
			_dockGeo =
				QByteArray::fromBase64(QByteArray(geometryStr));
		}
	}
	EnableDock(dockEnabled);
	obs_data_release(dockSettings);
}

void Macro::EnableDock(bool value)
{
	if (_registerDock == value) {
		return;
	}

	// Reset dock regardless
	RemoveDock();

	// Unregister dock
	if (_registerDock) {
		_dockIsFloating = true;
		_dockGeo = QByteArray();
		_registerDock = value;
		return;
	}

	// Create new dock widget
	auto window =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	_dock = new MacroDock(this, window,
			      QString::fromStdString(_runButtonText),
			      QString::fromStdString(_pauseButtonText),
			      QString::fromStdString(_unpauseButtonText));
	SetDockWidgetName(); // Used by OBS to restore position

	// Register new dock
	_dockAction = static_cast<QAction *>(obs_frontend_add_dock(_dock));

	// Note that OBSBasic::OBSInit() has precedence over the visibility and
	// geometry set here.
	// The function calls here are only intended to attempt to restore the
	// dock status when switching scene collections.
	if (switcher->startupLoadDone) {
		_dock->setVisible(_dockIsVisible);
		if (window->dockWidgetArea(_dock) != _dockArea) {
			window->addDockWidget(_dockArea, _dock);
		}
		if (_dock->isFloating() != _dockIsFloating) {
			_dock->setFloating(_dockIsFloating);
		}
		_dock->restoreGeometry(_dockGeo);
	}
	_registerDock = value;
}

void Macro::SetDockHasRunButton(bool value)
{
	_dockHasRunButton = value;
	if (!_dock) {
		return;
	}
	_dock->ShowRunButton(value);
}

void Macro::SetDockHasPauseButton(bool value)
{
	_dockHasPauseButton = value;
	if (!_dock) {
		return;
	}
	_dock->ShowPauseButton(value);
}

void Macro::SetRunButtonText(const std::string &text)
{
	_runButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetRunButtonText(QString::fromStdString(text));
}

void Macro::SetPauseButtonText(const std::string &text)
{
	_pauseButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetPauseButtonText(QString::fromStdString(text));
}

void Macro::SetUnpauseButtonText(const std::string &text)
{
	_unpauseButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetUnpauseButtonText(QString::fromStdString(text));
}

void Macro::RemoveDock()
{
	if (_dock) {
		_dock->close();
		_dock->deleteLater();
		_dockAction->deleteLater();
		_dock = nullptr;
		_dockAction = nullptr;
	}
}

void Macro::SetDockWidgetName() const
{
	if (!_dock) {
		return;
	}
	// Set prefix to avoid dock name conflict
	_dock->setObjectName("ADVSS-" + QString::fromStdString(_name));
	_dock->SetName(QString::fromStdString(_name));

	if (!_dockAction) {
		return;
	}
	_dockAction->setText(QString::fromStdString(_name));
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

static obs_hotkey_id registerHotkeyHelper(const std::string prefix,
					  const char *formatModuleText,
					  Macro *macro, obs_hotkey_func func)
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

void Macro::ClearHotkeys() const
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

void Macro::SetHotkeysDesc() const
{
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.pause", _name,
				   _pauseHotkey);
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.unpause",
				   _name, _unpauseHotkey);
	setHotkeyDescriptionHelper("AdvSceneSwitcher.hotkey.macro.togglePause",
				   _name, _togglePauseHotkey);
}

void SwitcherData::SaveMacros(obs_data_t *obj)
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

void SwitcherData::LoadMacros(obs_data_t *obj)
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

	int groupCount = 0;
	std::shared_ptr<Macro> group;
	std::vector<std::shared_ptr<Macro>> invalidGroups;
	for (auto &m : macros) {
		if (groupCount && m->IsGroup()) {
			blog(LOG_ERROR,
			     "nested group detected - will delete \"%s\"",
			     m->Name().c_str());
			invalidGroups.emplace_back(m);
			continue;
		}
		if (groupCount) {
			m->SetParent(group);
			groupCount--;
		}
		if (m->IsGroup()) {
			groupCount = m->GroupSize();
			group = m;
		}
		m->PostLoad();
	}

	if (groupCount) {
		blog(LOG_ERROR,
		     "invalid group size detected - will delete \"%s\"",
		     group->Name().c_str());
		invalidGroups.emplace_back(group);
	}

	for (auto &m : invalidGroups) {
		auto it = std::find(macros.begin(), macros.end(), m);
		if (it == macros.end()) {
			continue;
		}
		macros.erase(it);
	}
}

bool SwitcherData::CheckMacros()
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

bool SwitcherData::RunMacros()
{
	// Create copy of macor list as elements might be removed, inserted, or
	// reordered while macros are currently being executed.
	// For example, this can happen if a macro is performing a wait action,
	// as the main lock will be unlocked during this time.
	auto runPhaseMacros = macros;

	// Avoid deadlocks when opening settings window and calling frontend
	// API functions at the same time.
	//
	// If the timing is just right, the frontend API call will call
	// QMetaObject::invokeMethod(...) with  Qt::BlockingQueuedConnection
	// while holding the main switcher mutex.
	// But this invokeMethod call itself will be blocked as it is waiting
	// the constructor of AdvSceneSwitcher() to complete.
	// The constructor of AdvSceneSwitcher() cannot continue however as it
	// cannot lock the main switcher mutex.
	if (GetLock()) {
		GetLock()->unlock();
	}

	for (auto &m : runPhaseMacros) {
		if (m && m->Matched()) {
			vblog(LOG_INFO, "running macro: %s", m->Name().c_str());
			if (!m->PerformActions()) {
				blog(LOG_WARNING, "abort macro: %s",
				     m->Name().c_str());
			}
		}
	}
	if (GetLock()) {
		GetLock()->lock();
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

std::weak_ptr<Macro> GetWeakMacroByName(const char *name)
{
	for (auto &m : switcher->macros) {
		if (m->Name() == name) {
			return m;
		}
	}

	return {};
}

} // namespace advss
