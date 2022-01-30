#pragma once
#include "macro-segment.hpp"
#include "duration-control.hpp"

#include <QString>
#include <string>
#include <deque>
#include <memory>
#include <map>
#include <thread>
#include <obs.hpp>
#include <obs-module.h>

constexpr auto macro_func = 10;

constexpr auto logic_root_offset = 100;

enum class LogicType {
	ROOT_NONE = 0,
	ROOT_NOT,
	ROOT_LAST,
	// leave some space for potential expansion
	NONE = 100,
	AND,
	OR,
	AND_NOT,
	OR_NOT,
	LAST,
};

static inline bool isRootLogicType(LogicType l)
{
	return static_cast<int>(l) < logic_root_offset;
}

struct LogicTypeInfo {
	std::string _name;
};

class MacroCondition : public MacroSegment {
public:
	MacroCondition(Macro *m) : MacroSegment(m) {}
	virtual bool CheckCondition() = 0;
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	LogicType GetLogicType() { return _logic; }
	void SetLogicType(LogicType logic) { _logic = logic; }
	static const std::map<LogicType, LogicTypeInfo> logicTypes;

	bool DurationReached() { return _duration.DurationReached(); }
	void ResetDuration() { _duration.Reset(); }
	DurationConstraint GetDurationConstraint() { return _duration; }
	void SetDurationConstraint(const DurationConstraint &dur);
	void SetDurationCondition(DurationCondition cond);
	void SetDurationUnit(DurationUnit u);
	void SetDuration(double seconds);

private:
	LogicType _logic = LogicType::ROOT_NONE;
	DurationConstraint _duration;
};

class MacroAction : public MacroSegment {
public:
	MacroAction(Macro *m) : MacroSegment(m) {}
	virtual bool PerformAction() = 0;
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual void LogAction();
};

class Macro {
public:
	Macro(const std::string &name = "");
	virtual ~Macro();

	bool CeckMatch();
	bool PerformActions(bool forceParallel = false,
			    bool ignorePause = false);
	bool Matched() { return _matched; }
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
	void Stop() { _stop = true; }
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

private:
	void SetupHotkeys();
	void ClearHotkeys();
	void SetHotkeysDesc();
	void ResetTimers();
	void RunActions(bool &ret, bool ignorePause);
	void RunActions(bool ignorePause);

	std::string _name = "";
	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;
	bool _runInParallel = false;
	bool _matched = false;
	bool _lastMatched = false;
	bool _matchOnChange = false;
	bool _paused = false;
	int _count = 0;
	obs_hotkey_id _pauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _unpauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _togglePauseHotkey = OBS_INVALID_HOTKEY_ID;

	bool _wasExecutedRecently = false;

	bool _die = false;
	bool _stop = false;
	bool _done = true;
	std::thread _thread;
};

Macro *GetMacroByName(const char *name);
Macro *GetMacroByQString(const QString &name);

class MacroRef {
public:
	MacroRef(){};
	MacroRef(std::string name);
	void UpdateRef();
	void UpdateRef(std::string name);
	void UpdateRef(QString name);
	void Save(obs_data_t *obj);
	void Load(obs_data_t *obj);
	Macro *get();
	Macro *operator->();

private:
	std::string _name = "";
	Macro *_ref = nullptr;
};

class MacroRefCondition : public MacroCondition {
public:
	MacroRefCondition(Macro *m) : MacroCondition(m) {}
	void ResolveMacroRef();
	MacroRef _macro;
};

class MacroRefAction : public MacroAction {
public:
	MacroRefAction(Macro *m) : MacroAction(m) {}
	void ResolveMacroRef();
	MacroRef _macro;
};

class MultiMacroRefAction : public MacroAction {
public:
	MultiMacroRefAction(Macro *m) : MacroAction(m) {}
	void ResolveMacroRef();
	std::vector<MacroRef> _macros;
};
