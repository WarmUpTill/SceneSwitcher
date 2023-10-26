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

	// Keep the dock widgets in case of shutdown so they can be restored by
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

	_conditionSateChanged = _lastMatched != _matched;
	if (!_conditionSateChanged && _performActionsOnChange) {
		_onPreventedActionExecution = true;
	}
	_lastMatched = _matched;
	_lastCheckTime = std::chrono::high_resolution_clock::now();
	return _matched;
}

bool Macro::PerformActions(bool match, bool forceParallel, bool ignorePause)
{
	if (!_done) {
		vblog(LOG_INFO, "macro %s already running", _name.c_str());
		return !forceParallel;
	}
	std::function<bool(bool)> runFunc =
		match ? std::bind(&Macro::RunActions, this,
				  std::placeholders::_1)
		      : std::bind(&Macro::RunElseActions, this,
				  std::placeholders::_1);
	_stop = false;
	_done = false;
	bool ret = true;
	if (_runInParallel || forceParallel) {
		if (_backgroundThread.joinable()) {
			_backgroundThread.join();
		}
		_backgroundThread = std::thread(
			[this, runFunc, ignorePause] { runFunc(ignorePause); });
	} else {
		ret = runFunc(ignorePause);
	}
	_lastExecutionTime = std::chrono::high_resolution_clock::now();
	auto group = _parent.lock();
	if (group) {
		group->_lastExecutionTime = _lastExecutionTime;
	}
	if (_runCount != std::numeric_limits<int>::max()) {
		_runCount++;
	}
	return ret;
}

bool Macro::ExecutedSince(
	const std::chrono::high_resolution_clock::time_point &time)
{
	return _lastExecutionTime > time;
}

bool Macro::ShouldRunActions() const
{
	const bool hasActionsToExecute =
		(_matched || _elseActions.size() > 0) &&
		(!_performActionsOnChange || _conditionSateChanged);

	if (VerboseLoggingEnabled() && _performActionsOnChange &&
	    !_conditionSateChanged) {
		if (_matched && _actions.size() > 0) {
			blog(LOG_INFO, "skip actions for Macro %s (on change)",
			     _name.c_str());
		}
		if (!_matched && _elseActions.size() > 0) {
			blog(LOG_INFO,
			     "skip else actions for Macro %s (on change)",
			     _name.c_str());
		}
	}

	return hasActionsToExecute;
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
	_lastExecutionTime = {};
}

bool Macro::RunActionsHelper(
	const std::deque<std::shared_ptr<MacroAction>> &actionsToRun,
	bool ignorePause)
{
	// Create copy of action list as elements might be removed, inserted, or
	// reordered while actions are currently being executed.
	auto actions = actionsToRun;

	bool actionsExecutedSuccessfully = true;
	for (auto &action : actions) {
		if (action->Enabled()) {
			action->LogAction();
			actionsExecutedSuccessfully =
				actionsExecutedSuccessfully &&
				action->PerformAction();
		} else {
			vblog(LOG_INFO, "skipping disabled action %s",
			      action->GetId().c_str());
		}
		if (!actionsExecutedSuccessfully || (_paused && !ignorePause) ||
		    _stop || _die) {
			break;
		}
		if (action->Enabled()) {
			action->SetHighlight();
		}
	}
	_done = true;
	return actionsExecutedSuccessfully;
}

bool Macro::RunActions(bool ignorePause)
{
	vblog(LOG_INFO, "running actions of %s", _name.c_str());
	return RunActionsHelper(_actions, ignorePause);
}

bool Macro::RunElseActions(bool ignorePause)
{
	vblog(LOG_INFO, "running else actions of %s", _name.c_str());
	return RunActionsHelper(_elseActions, ignorePause);
}

bool Macro::DockIsVisible() const
{
	return _dock && _dockAction && _dock->isVisible();
}

void Macro::SetMatchOnChange(bool onChange)
{
	_performActionsOnChange = onChange;
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

std::deque<std::shared_ptr<MacroAction>> &Macro::ElseActions()
{
	return _elseActions;
}

static void updateIndicesHelper(std::deque<std::shared_ptr<MacroSegment>> &list)
{
	int idx = 0;
	for (auto segment : list) {
		segment->SetIndex(idx);
		idx++;
	}
}

void Macro::UpdateActionIndices()
{
	std::deque<std::shared_ptr<MacroSegment>> list(_actions.begin(),
						       _actions.end());
	updateIndicesHelper(list);
}

void Macro::UpdateElseActionIndices()
{
	std::deque<std::shared_ptr<MacroSegment>> list(_elseActions.begin(),
						       _elseActions.end());
	updateIndicesHelper(list);
}

void Macro::UpdateConditionIndices()
{
	std::deque<std::shared_ptr<MacroSegment>> list(_conditions.begin(),
						       _conditions.end());
	updateIndicesHelper(list);
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
	obs_data_set_bool(obj, "onChange", _performActionsOnChange);
	obs_data_set_bool(obj, "skipExecOnStart", _skipExecOnStart);

	obs_data_set_bool(obj, "group", _isGroup);
	if (_isGroup) {
		OBSDataAutoRelease groupData = obs_data_create();
		obs_data_set_bool(groupData, "collapsed", _isCollapsed);
		obs_data_set_int(groupData, "size", _groupSize);
		obs_data_set_obj(obj, "groupData", groupData);
		return true;
	}

	SaveDockSettings(obj);

	SaveSplitterPos(_actionConditionSplitterPosition, obj,
			"macroActionConditionSplitterPosition");
	SaveSplitterPos(_elseActionSplitterPosition, obj,
			"macroElseActionSplitterPosition");

	obs_data_set_bool(obj, "registerHotkeys", _registerHotkeys);
	OBSDataArrayAutoRelease pauseHotkey = obs_hotkey_save(_pauseHotkey);
	obs_data_set_array(obj, "pauseHotkey", pauseHotkey);
	OBSDataArrayAutoRelease unpauseHotkey = obs_hotkey_save(_unpauseHotkey);
	obs_data_set_array(obj, "unpauseHotkey", unpauseHotkey);
	OBSDataArrayAutoRelease togglePauseHotkey =
		obs_hotkey_save(_togglePauseHotkey);
	obs_data_set_array(obj, "togglePauseHotkey", togglePauseHotkey);

	OBSDataArrayAutoRelease conditions = obs_data_array_create();
	for (auto &c : _conditions) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		c->Save(arrayObj);
		obs_data_array_push_back(conditions, arrayObj);
	}
	obs_data_set_array(obj, "conditions", conditions);

	OBSDataArrayAutoRelease actions = obs_data_array_create();
	for (auto &a : _actions) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		a->Save(arrayObj);
		obs_data_array_push_back(actions, arrayObj);
	}
	obs_data_set_array(obj, "actions", actions);

	OBSDataArrayAutoRelease elseActions = obs_data_array_create();
	for (auto &a : _elseActions) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		a->Save(arrayObj);
		obs_data_array_push_back(elseActions, arrayObj);
	}
	obs_data_set_array(obj, "elseActions", elseActions);

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
	_performActionsOnChange = obs_data_get_bool(obj, "onChange");
	_skipExecOnStart = obs_data_get_bool(obj, "skipExecOnStart");

	_isGroup = obs_data_get_bool(obj, "group");
	if (_isGroup) {
		OBSDataAutoRelease groupData =
			obs_data_get_obj(obj, "groupData");
		_isCollapsed = obs_data_get_bool(groupData, "collapsed");
		_groupSize = obs_data_get_int(groupData, "size");
		return true;
	}

	LoadDockSettings(obj);

	LoadSplitterPos(_actionConditionSplitterPosition, obj,
			"macroActionConditionSplitterPosition");
	LoadSplitterPos(_elseActionSplitterPosition, obj,
			"macroElseActionSplitterPosition");

	obs_data_set_default_bool(obj, "registerHotkeys", true);
	_registerHotkeys = obs_data_get_bool(obj, "registerHotkeys");
	if (_registerHotkeys) {
		SetupHotkeys();
	}
	OBSDataArrayAutoRelease pauseHotkey =
		obs_data_get_array(obj, "pauseHotkey");
	obs_hotkey_load(_pauseHotkey, pauseHotkey);
	OBSDataArrayAutoRelease unpauseHotkey =
		obs_data_get_array(obj, "unpauseHotkey");
	obs_hotkey_load(_unpauseHotkey, unpauseHotkey);
	OBSDataArrayAutoRelease togglePauseHotkey =
		obs_data_get_array(obj, "togglePauseHotkey");
	obs_hotkey_load(_togglePauseHotkey, togglePauseHotkey);
	SetHotkeysDesc();

	bool root = true;
	OBSDataArrayAutoRelease conditions =
		obs_data_get_array(obj, "conditions");
	size_t count = obs_data_array_count(conditions);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj =
			obs_data_array_item(conditions, i);
		std::string id = obs_data_get_string(arrayObj, "id");
		auto newEntry = MacroConditionFactory::Create(id, this);
		if (newEntry) {
			_conditions.emplace_back(newEntry);
			auto c = _conditions.back().get();
			c->Load(arrayObj);
			setValidLogic(c, root, _name);
		} else {
			blog(LOG_WARNING,
			     "discarding condition entry with unknown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}
		root = false;
	}
	UpdateConditionIndices();

	OBSDataArrayAutoRelease actions = obs_data_get_array(obj, "actions");
	count = obs_data_array_count(actions);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease array_obj = obs_data_array_item(actions, i);
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
	}
	UpdateActionIndices();

	OBSDataArrayAutoRelease elseActions =
		obs_data_get_array(obj, "elseActions");
	count = obs_data_array_count(elseActions);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease array_obj =
			obs_data_array_item(elseActions, i);
		std::string id = obs_data_get_string(array_obj, "id");
		auto newEntry = MacroActionFactory::Create(id, this);
		if (newEntry) {
			_elseActions.emplace_back(newEntry);
			_elseActions.back()->Load(array_obj);
		} else {
			blog(LOG_WARNING,
			     "discarding elseAction entry with unknown id (%s) for macro %s",
			     id.c_str(), _name.c_str());
		}
	}
	UpdateElseActionIndices();
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
	for (auto &a : _elseActions) {
		a->PostLoad();
	}
	return true;
}

bool Macro::SwitchesScene() const
{
	MacroActionSwitchScene temp(nullptr);
	auto sceneSwitchId = temp.GetId();
	for (const auto &a : _actions) {
		if (a->GetId() == sceneSwitchId) {
			return true;
		}
	}
	for (const auto &a : _elseActions) {
		if (a->GetId() == sceneSwitchId) {
			return true;
		}
	}
	return false;
}

const QList<int> &Macro::GetActionConditionSplitterPosition() const
{
	return _actionConditionSplitterPosition;
}

void Macro::SetActionConditionSplitterPosition(const QList<int> sizes)
{
	_actionConditionSplitterPosition = sizes;
}

const QList<int> &Macro::GetElseActionSplitterPosition() const
{
	return _elseActionSplitterPosition;
}

void Macro::SetElseActionSplitterPosition(const QList<int> sizes)
{
	_elseActionSplitterPosition = sizes;
}

bool Macro::HasValidSplitterPositions() const
{
	return !_actionConditionSplitterPosition.empty() &&
	       !_elseActionSplitterPosition.empty();
}

bool Macro::OnChangePreventedActionsRecently()
{
	if (_onPreventedActionExecution) {
		_onPreventedActionExecution = false;
		return _matched ? _actions.size() > 0 : _elseActions.size() > 0;
	}
	return false;
}

void Macro::ResetUIHelpers()
{
	_onPreventedActionExecution = false;
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
	obs_data_set_bool(dockSettings, "hasStatusLabel", _dockHasStatusLabel);
	obs_data_set_bool(dockSettings, "highlightIfConditionsTrue",
			  _dockHighlight);
	_runButtonText.Save(dockSettings, "runButtonText");
	_pauseButtonText.Save(dockSettings, "pauseButtonText");
	_unpauseButtonText.Save(dockSettings, "unpauseButtonText");
	_conditionsTrueStatusText.Save(dockSettings,
				       "conditionsTrueStatusText");
	_conditionsFalseStatusText.Save(dockSettings,
					"conditionsFalseStatusText");
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
	_runButtonText.Load(dockSettings, "runButtonText");
	_pauseButtonText.Load(dockSettings, "pauseButtonText");
	_unpauseButtonText.Load(dockSettings, "unpauseButtonText");
	_conditionsTrueStatusText.Load(dockSettings,
				       "conditionsTrueStatusText");
	_conditionsFalseStatusText.Load(dockSettings,
					"conditionsFalseStatusText");
	if (dockEnabled) {
		_dockHasRunButton =
			obs_data_get_bool(dockSettings, "hasRunButton");
		_dockHasPauseButton =
			obs_data_get_bool(dockSettings, "hasPauseButton");
		_dockHasStatusLabel =
			obs_data_get_bool(dockSettings, "hasStatusLabel");
		_dockHighlight = obs_data_get_bool(dockSettings,
						   "highlightIfConditionsTrue");
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
	_dock = new MacroDock(GetWeakMacroByName(_name.c_str()), window,
			      _runButtonText, _pauseButtonText,
			      _unpauseButtonText, _conditionsTrueStatusText,
			      _conditionsFalseStatusText, _dockHighlight);
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

void Macro::SetDockHasStatusLabel(bool value)
{
	_dockHasStatusLabel = value;
	if (!_dock) {
		return;
	}
	_dock->ShowStatusLabel(value);
}

void Macro::SetHighlightEnable(bool value)
{
	_dockHighlight = value;
	if (!_dock) {
		return;
	}
	_dock->EnableHighlight(value);
}

void Macro::SetRunButtonText(const std::string &text)
{
	_runButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetRunButtonText(text);
}

void Macro::SetPauseButtonText(const std::string &text)
{
	_pauseButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetPauseButtonText(text);
}

void Macro::SetUnpauseButtonText(const std::string &text)
{
	_unpauseButtonText = text;
	if (!_dock) {
		return;
	}
	_dock->SetUnpauseButtonText(text);
}

void Macro::SetConditionsTrueStatusText(const std::string &text)
{
	_conditionsTrueStatusText = text;
	if (!_dock) {
		return;
	}
	_dock->SetConditionsTrueText(text);
}

StringVariable Macro::ConditionsTrueStatusText() const
{
	return _conditionsTrueStatusText;
}

void Macro::SetConditionsFalseStatusText(const std::string &text)
{
	_conditionsFalseStatusText = text;
	if (!_dock) {
		return;
	}
	_dock->SetConditionsFalseText(text);
}

StringVariable Macro::ConditionsFalseStatusText() const
{
	return _conditionsFalseStatusText;
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
		if (m->CeckMatch() || m->ElseActions().size() > 0) {
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
	// Create copy of macro list as elements might be removed, inserted, or
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
		if (!m || !m->ShouldRunActions()) {
			continue;
		}
		if (firstInterval && m->SkipExecOnStart()) {
			blog(LOG_INFO,
			     "skip execution of macro \"%s\" at startup",
			     m->Name().c_str());
			continue;
		}
		vblog(LOG_INFO, "running macro: %s", m->Name().c_str());
		if (!m->PerformActions(m->Matched())) {
			blog(LOG_WARNING, "abort macro: %s", m->Name().c_str());
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
