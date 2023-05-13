#pragma once
#include "macro-action.hpp"
#include "macro-condition.hpp"
#include "macro-ref.hpp"

#include <QString>
#include <QByteArray>
#include <string>
#include <deque>
#include <memory>
#include <map>
#include <thread>
#include <obs.hpp>
#include <obs-module.h>

namespace advss {

constexpr auto macro_func = 10;

class MacroDock;

class Macro {
public:
	Macro(const std::string &name = "", const bool addHotkey = false);
	virtual ~Macro();
	bool CeckMatch();
	bool PerformActions(bool forceParallel = false,
			    bool ignorePause = false);
	bool Matched() const { return _matched; }
	int64_t MsSinceLastCheck() const;
	std::string Name() const { return _name; }
	void SetName(const std::string &name);
	void SetRunInParallel(bool parallel) { _runInParallel = parallel; }
	bool RunInParallel() const { return _runInParallel; }
	void SetPaused(bool pause = true);
	bool Paused() const { return _paused; }
	void SetMatchOnChange(bool onChange) { _matchOnChange = onChange; }
	bool MatchOnChange() const { return _matchOnChange; }
	int RunCount() const { return _runCount; };
	void ResetRunCount() { _runCount = 0; };
	void ResetTimers();
	void AddHelperThread(std::thread &&);
	bool GetStop() const { return _stop; }
	void Stop();

	std::deque<std::shared_ptr<MacroCondition>> &Conditions();
	std::deque<std::shared_ptr<MacroAction>> &Actions();
	void UpdateActionIndices();
	void UpdateConditionIndices();

	// Group controls
	static std::shared_ptr<Macro>
	CreateGroup(const std::string &name,
		    std::vector<std::shared_ptr<Macro>> &children);
	static void RemoveGroup(std::shared_ptr<Macro>);
	static void PrepareMoveToGroup(Macro *group,
				       std::shared_ptr<Macro> item);
	static void PrepareMoveToGroup(std::shared_ptr<Macro> group,
				       std::shared_ptr<Macro> item);
	bool IsGroup() const { return _isGroup; }
	uint32_t GroupSize() const { return _groupSize; }
	bool IsSubitem() const { return !_parent.expired(); }
	void SetCollapsed(bool val) { _isCollapsed = val; }
	bool IsCollapsed() const { return _isCollapsed; }
	void SetParent(std::shared_ptr<Macro> m) { _parent = m; }
	std::shared_ptr<Macro> Parent() const;

	// Saving and loading
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	// Some macros can refer to other macros, which are not yet loaded.
	// Use this function to set these references after loading is complete.
	bool PostLoad();

	// Helper function for plugin state condition regarding scene change
	bool SwitchesScene() const;

	// UI helpers
	bool WasExecutedRecently();
	bool OnChangePreventedActionsRecently();
	void ResetUIHelpers();

	void EnablePauseHotkeys(bool);
	bool PauseHotkeysEnabled() const;

	void EnableDock(bool);
	bool DockEnabled() const { return _registerDock; }
	void SetDockHasRunButton(bool value);
	bool DockHasRunButton() const { return _dockHasRunButton; }
	void SetDockHasPauseButton(bool value);
	bool DockHasPauseButton() const { return _dockHasPauseButton; }
	std::string RunButtonText() const { return _runButtonText; }
	void SetRunButtonText(const std::string &text);
	std::string PauseButtonText() const { return _pauseButtonText; }
	void SetPauseButtonText(const std::string &text);
	std::string UnpauseButtonText() const { return _unpauseButtonText; }
	void SetUnpauseButtonText(const std::string &text);

private:
	void SetupHotkeys();
	void ClearHotkeys() const;
	void SetHotkeysDesc() const;
	void RunActions(bool &ret, bool ignorePause);
	void RunActions(bool ignorePause);
	void SetOnChangeHighlight();
	bool DockIsVisible() const;
	void SetDockWidgetName() const;
	void SaveDockSettings(obs_data_t *obj) const;
	void LoadDockSettings(obs_data_t *obj);
	void RemoveDock();

	std::string _name = "";
	bool _die = false;
	bool _stop = false;
	bool _done = true;
	std::chrono::high_resolution_clock::time_point _lastCheckTime{};
	std::thread _backgroundThread;
	std::vector<std::thread> _helperThreads;

	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;

	std::weak_ptr<Macro> _parent;
	uint32_t _groupSize = 0;
	bool _isGroup = false;
	bool _isCollapsed = false;

	bool _runInParallel = false;
	bool _matched = false;
	bool _lastMatched = false;
	bool _matchOnChange = false;
	bool _paused = false;
	int _runCount = 0;
	bool _registerHotkeys = true;
	obs_hotkey_id _pauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _unpauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _togglePauseHotkey = OBS_INVALID_HOTKEY_ID;

	// UI helpers for the macro tab
	bool _wasExecutedRecently = false;
	bool _onChangeTriggered = false;

	bool _registerDock = false;
	bool _dockHasRunButton = true;
	bool _dockHasPauseButton = true;
	std::string _runButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.run");
	std::string _pauseButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.pause");
	std::string _unpauseButtonText =
		obs_module_text("AdvSceneSwitcher.macroDock.unpause");
	bool _dockIsFloating = true;
	bool _dockIsVisible = false;
	Qt::DockWidgetArea _dockArea;
	QByteArray _dockGeo;
	MacroDock *_dock = nullptr;
	QAction *_dockAction = nullptr;
};

Macro *GetMacroByName(const char *name);
Macro *GetMacroByQString(const QString &name);
std::weak_ptr<Macro> GetWeakMacroByName(const char *name);

} // namespace advss
