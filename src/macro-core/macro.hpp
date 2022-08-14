#pragma once
#include "macro-action.hpp"
#include "macro-condition.hpp"
#include "macro-ref.hpp"

#include <QString>
#include <string>
#include <deque>
#include <memory>
#include <map>
#include <thread>
#include <obs.hpp>
#include <obs-module.h>

constexpr auto macro_func = 10;

class Macro {
public:
	Macro(const std::string &name = "", const bool addHotkey = false);
	virtual ~Macro();

	bool CeckMatch();
	bool PerformActions(bool forceParallel = false,
			    bool ignorePause = false);
	bool Matched() { return _matched; }
	int64_t MsSinceLastCheck();
	std::string Name() { return _name; }
	void SetName(const std::string &name);
	void SetRunInParallel(bool parallel) { _runInParallel = parallel; }
	bool RunInParallel() { return _runInParallel; }
	void SetPaused(bool pause = true);
	bool Paused() { return _paused; }
	void SetMatchOnChange(bool onChange) { _matchOnChange = onChange; }
	bool MatchOnChange() { return _matchOnChange; }
	int GetCount() { return _count; };
	void ResetCount() { _count = 0; };
	void AddHelperThread(std::thread &&);
	bool GetStop() { return _stop; }
	void Stop();
	std::deque<std::shared_ptr<MacroCondition>> &Conditions()
	{
		return _conditions;
	}
	void UpdateActionIndices();
	void UpdateConditionIndices();
	std::deque<std::shared_ptr<MacroAction>> &Actions() { return _actions; }

	bool Save(obs_data_t *obj);
	bool Load(obs_data_t *obj);
	// Some macros can refer to other macros, which are not yet loaded.
	// Use this function to set these references after loading is complete.
	void ResolveMacroRef();

	// Helper function for plugin state condition regarding scene change
	bool SwitchesScene();

	// UI helpers for the macro tab
	bool WasExecutedRecently();
	bool OnChangePreventedActionsRecently();
	void ResetUIHelpers();

	void EnablePauseHotkeys(bool);
	bool PauseHotkeysEnabled();

private:
	void SetupHotkeys();
	void ClearHotkeys();
	void SetHotkeysDesc();
	void ResetTimers();
	void RunActions(bool &ret, bool ignorePause);
	void RunActions(bool ignorePause);
	void SetOnChangeHighlight();

	std::string _name = "";
	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;
	bool _runInParallel = false;
	bool _matched = false;
	bool _lastMatched = false;
	bool _matchOnChange = false;
	bool _paused = false;
	int _count = 0;
	bool _registerHotkeys = true;
	obs_hotkey_id _pauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _unpauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _togglePauseHotkey = OBS_INVALID_HOTKEY_ID;

	// UI helpers for the macro tab
	bool _wasExecutedRecently = false;
	bool _onChangeTriggered = false;

	std::chrono::high_resolution_clock::time_point _lastCheckTime{};

	bool _die = false;
	bool _stop = false;
	bool _done = true;
	std::thread _backgroundThread;
	std::vector<std::thread> _helperThreads;
};

Macro *GetMacroByName(const char *name);
Macro *GetMacroByQString(const QString &name);
