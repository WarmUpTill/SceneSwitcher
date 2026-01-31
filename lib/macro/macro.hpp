#pragma once
#include "macro-action.hpp"
#include "macro-condition.hpp"
#include "macro-dock-settings.hpp"
#include "macro-helpers.hpp"
#include "macro-input.hpp"
#include "macro-ref.hpp"
#include "variable-string.hpp"
#include "temp-variable.hpp"

#include <future>
#include <string>
#include <deque>
#include <memory>
#include <map>
#include <thread>
#include <obs.hpp>
#include <obs-module-helper.hpp>

namespace advss {

class GlobalMacroSettings;

class Macro {
	using TimePoint = std::chrono::high_resolution_clock::time_point;

public:
	enum class PauseStateSaveBehavior { PERSIST, PAUSE, UNPAUSE };

	Macro(const std::string &name = "");
	Macro(const std::string &name, const GlobalMacroSettings &settings);
	~Macro();

	std::string Name() const { return _name; }
	void SetName(const std::string &name);

	bool CheckConditions(bool ignorePause = false);
	bool ConditionsMatched() const { return _matched; }
	TimePoint LastConditionCheckTime() const { return _lastCheckTime; }
	bool ConditionsShouldBeChecked() const;

	bool ShouldRunActions() const;
	bool PerformActions(bool match, bool forceParallel = false,
			    bool ignorePause = false);

	void SetPaused(bool pause = true);
	bool Paused() const { return _paused; }
	bool WasPausedSince(const TimePoint &) const;

	void SetPauseStateSaveBehavior(PauseStateSaveBehavior);
	PauseStateSaveBehavior GetPauseStateSaveBehavior() const;

	void Stop();
	bool GetStop() const { return _stop; }
	void ResetTimers();

	void SetMatchOnChange(bool onChange);
	bool MatchOnChange() const { return _performActionsOnChange; }

	void SetSkipExecOnStart(bool skip) { _skipExecOnStart = skip; }
	bool SkipExecOnStart() const { return _skipExecOnStart; }

	void SetStopActionsIfNotDone(bool stopActionsIfNotDone);
	bool StopActionsIfNotDone() const { return _stopActionsIfNotDone; }

	void SetShortCircuitEvaluation(bool useShortCircuitEvaluation);
	bool ShortCircuitEvaluationEnabled() const;

	void SetCustomConditionCheckIntervalEnabled(bool enable);
	bool CustomConditionCheckIntervalEnabled() const;
	void SetCustomConditionCheckInterval(const Duration &);
	Duration GetCustomConditionCheckInterval() const;

	int RunCount() const { return _runCount; };
	void ResetRunCount() { _runCount = 0; };

	void AddHelperThread(std::thread &&);
	void SetRunInParallel(bool parallel) { _runInParallel = parallel; }
	bool RunInParallel() const { return _runInParallel; }
	bool CheckInParallel() const { return _checkInParallel; }
	void SetCheckInParallel(bool parallel);
	bool ParallelTasksCompleted() const;

	// Input variables
	MacroInputVariables GetInputVariables() const;
	void SetInputVariables(const MacroInputVariables &);

	// Temporary variable helpers
	std::vector<TempVariable> GetTempVars(const MacroSegment *filter) const;
	std::optional<const TempVariable>
	GetTempVar(const MacroSegment *, const std::string &id) const;
	void InvalidateTempVarValues() const;

	// Macro segments
	std::deque<std::shared_ptr<MacroCondition>> &Conditions();
	const std::deque<std::shared_ptr<MacroCondition>> &Conditions() const;
	std::deque<std::shared_ptr<MacroAction>> &Actions();
	const std::deque<std::shared_ptr<MacroAction>> &Actions() const;
	std::deque<std::shared_ptr<MacroAction>> &ElseActions();
	const std::deque<std::shared_ptr<MacroAction>> &ElseActions() const;
	void UpdateActionIndices();
	void UpdateElseActionIndices();
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
	void ResetGroupSize() { _groupSize = 0; }
	bool IsSubitem() const { return !_parent.expired(); }
	void SetCollapsed(bool val) { _isCollapsed = val; }
	bool IsCollapsed() const { return _isCollapsed; }
	void SetParent(std::shared_ptr<Macro> m) { _parent = m; }
	std::shared_ptr<Macro> Parent() const;

	// Saving and loading
	bool Save(obs_data_t *obj, bool saveForCopy = false) const;
	bool Load(obs_data_t *obj);
	// Some macros can refer to other macros, which are not yet loaded.
	// Use this function to set these references after loading is complete.
	bool PostLoad();

	// Helper function for plugin state condition regarding scene change
	bool SwitchesScene() const;

	// UI helpers
	void SetActionConditionSplitterPosition(const QList<int>);
	const QList<int> &GetActionConditionSplitterPosition() const;
	void SetElseActionSplitterPosition(const QList<int>);
	const QList<int> &GetElseActionSplitterPosition() const;
	bool HasValidSplitterPositions() const;
	bool WasExecutedSince(const TimePoint &) const;
	bool OnChangePreventedActionsSince(const TimePoint &) const;
	TimePoint GetLastExecutionTime() const;
	void ResetUIHelpers();

	// Hotkeys
	void EnablePauseHotkeys(bool);
	bool PauseHotkeysEnabled() const;

	// Docks
	MacroDockSettings &GetDockSettings() { return _dockSettings; }

private:
	void SetupHotkeys();
	void ClearHotkeys() const;
	void SetHotkeysDesc() const;

	bool
	CheckConditionHelper(const std::shared_ptr<MacroCondition> &) const;

	bool RunActionsHelper(
		const std::deque<std::shared_ptr<MacroAction>> &actions,
		bool ignorePause);
	bool RunActions(bool ignorePause);
	bool RunElseActions(bool ignorePause);

	std::string _name = "";
	bool _die = false;
	bool _stop = false;
	std::future<void> _actionRunFuture;
	TimePoint _lastCheckTime{};
	TimePoint _lastUnpauseTime{};
	TimePoint _lastExecutionTime{};
	TimePoint _lastOnChangeActionsPreventedTime{};
	std::vector<std::thread> _helperThreads;

	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;
	std::deque<std::shared_ptr<MacroAction>> _elseActions;

	std::weak_ptr<Macro> _parent;
	uint32_t _groupSize = 0;
	bool _isGroup = false;
	bool _isCollapsed = false;

	bool _useShortCircuitEvaluation = false;
	bool _useCustomConditionCheckInterval = false;
	Duration _customConditionCheckInterval = 0.3;
	bool _conditionSateChanged = false;

	bool _runInParallel = false;
	bool _checkInParallel = false;
	bool _matched = false;
	std::future<void> _conditionCheckFuture;
	bool _lastMatched = false;
	bool _performActionsOnChange = true;
	bool _skipExecOnStart = false;
	bool _stopActionsIfNotDone = false;
	bool _paused = false;
	int _runCount = 0;
	bool _registerHotkeys = true;
	obs_hotkey_id _pauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _unpauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _togglePauseHotkey = OBS_INVALID_HOTKEY_ID;

	PauseStateSaveBehavior _pauseSaveBehavior =
		PauseStateSaveBehavior::PERSIST;

	MacroInputVariables _inputVariables;

	QList<int> _actionConditionSplitterPosition;
	QList<int> _elseActionSplitterPosition;

	MacroDockSettings _dockSettings;
};

void LoadMacros(obs_data_t *obj);
void SaveMacros(obs_data_t *obj);
bool CheckMacros();
bool RunMacros();
void StopAllMacros();
void InvalidateMacroTempVarValues();
std::shared_ptr<Macro> GetMacroWithInvalidConditionInterval();

} // namespace advss
