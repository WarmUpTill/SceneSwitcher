#pragma once
#include "duration-control.hpp"

#include <string>
#include <deque>
#include <memory>
#include <map>
#include <obs.hpp>
#include <obs-module.h>
#include <QString>

constexpr auto macro_func = 10;
constexpr auto default_priority_10 = macro_func;

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

class MacroSegment {
public:
	void SetIndex(int idx) { _idx = idx; }
	int GetIndex() { return _idx; }
	void SetCollapsed(bool collapsed) { _collapsed = collapsed; }
	bool GetCollapsed() { return _collapsed; }
	virtual bool Save(obs_data_t *obj) = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	virtual std::string GetShortDesc();
	virtual std::string GetId() = 0;

protected:
	int _idx;
	bool _collapsed = false;
};

class MacroCondition : public MacroSegment {
public:
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
	LogicType _logic;
	DurationConstraint _duration;
};

class MacroAction : public MacroSegment {
public:
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
	bool PerformAction();
	bool Matched() { return _matched; }
	std::string Name() { return _name; }
	void SetName(const std::string &name);
	void SetPaused(bool pause = true) { _paused = pause; }
	bool Paused() { return _paused; }
	int GetCount() { return _count; };
	void ResetCount() { _count = 0; };
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

private:
	void SetupHotkeys();
	void ClearHotkeys();
	void SetHotkeysDesc();

	std::string _name = "";
	std::deque<std::shared_ptr<MacroCondition>> _conditions;
	std::deque<std::shared_ptr<MacroAction>> _actions;
	bool _matched = false;
	bool _paused = false;
	int _count = 0;
	obs_hotkey_id _pauseHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id _unpauseHotkey = OBS_INVALID_HOTKEY_ID;
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
	void ResolveMacroRef();
	MacroRef _macro;
};

class MacroRefAction : public MacroAction {
public:
	void ResolveMacroRef();
	MacroRef _macro;
};
