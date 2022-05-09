#pragma once
#include "macro-segment.hpp"
#include "macro-ref.hpp"
#include "duration-control.hpp"

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

enum class DurationCondition {
	NONE,
	MORE,
	EQUAL,
	LESS,
};

class DurationConstraint {
public:
	void Save(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");
	void Load(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *secondsName = "seconds",
		  const char *unitName = "displayUnit");
	void SetCondition(DurationCondition cond) { _type = cond; }
	void SetDuration(const Duration &dur) { _dur = dur; }
	void SetValue(double value) { _dur.seconds = value; }
	void SetUnit(DurationUnit u) { _dur.displayUnit = u; }
	DurationCondition GetCondition() { return _type; }
	Duration GetDuration() { return _dur; }
	bool DurationReached();
	void Reset();

private:
	DurationCondition _type = DurationCondition::NONE;
	Duration _dur;
	bool _timeReached = false;
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

class MacroRefCondition : public MacroCondition {
public:
	MacroRefCondition(Macro *m) : MacroCondition(m) {}
	void ResolveMacroRef();
	MacroRef _macro;
};
