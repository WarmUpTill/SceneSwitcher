#pragma once
#include "macro-segment.hpp"
#include "condition-logic.hpp"
#include "duration-control.hpp"
#include "macro-ref.hpp"

namespace advss {

class DurationModifier {
public:
	enum class Type {
		NONE,
		MORE,
		EQUAL,
		LESS,
		WITHIN, // Condition will remain true for a set a amount of time
			// regardless of current state
	};

	void Save(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *duration = "seconds") const;
	void Load(obs_data_t *obj, const char *condName = "time_constraint",
		  const char *duration = "seconds");
	void SetModifier(Type cond) { _type = cond; }
	void SetTimeRemaining(const double &val) { _dur.SetTimeRemaining(val); }
	void SetValue(const Duration &value) { _dur = value; }
	Type GetType() { return _type; }
	Duration GetDuration() { return _dur; }
	bool DurationReached();
	void Reset();

private:
	Type _type = Type::NONE;
	Duration _dur;
	bool _timeReached = false;
};

class EXPORT MacroCondition : public MacroSegment {
public:
	MacroCondition(Macro *m, bool supportsVariableValue = false);
	virtual ~MacroCondition() = default;
	virtual bool CheckCondition() = 0;
	virtual bool Save(obs_data_t *obj) const = 0;
	virtual bool Load(obs_data_t *obj) = 0;
	Logic::Type GetLogicType() const { return _logic.GetType(); }
	void SetLogicType(const Logic::Type &logic) { _logic.SetType(logic); }
	void ValidateLogicSelection(bool isRootCondition, const char *context);
	void ResetDuration();
	void CheckDurationModifier(bool &val);
	DurationModifier GetDurationModifier() { return _duration; }
	void SetDurationModifier(DurationModifier::Type m);
	void SetDuration(const Duration &duration);

	static std::string_view GetDefaultID();

private:
	Logic _logic = Logic(Logic::Type::ROOT_NONE);
	DurationModifier _duration;
};

class EXPORT MacroRefCondition : virtual public MacroCondition {
public:
	MacroRefCondition(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	MacroRef _macro;
};

class EXPORT MultiMacroRefCondition : virtual public MacroCondition {
public:
	MultiMacroRefCondition(Macro *m, bool supportsVariableValue = false);
	bool PostLoad() override;

	std::vector<MacroRef> _macros;
};

} // namespace advss
