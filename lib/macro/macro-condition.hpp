#pragma once
#include "macro-segment.hpp"
#include "condition-logic.hpp"
#include "duration-modifier.hpp"
#include "macro-ref.hpp"

namespace advss {

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

	DurationModifier GetDurationModifier() const;
	void SetDurationModifier(DurationModifier::Type m);
	void SetDuration(const Duration &duration);

	void ResetDuration();
	bool CheckDurationModifier(bool conditionValue);

	static std::string_view GetDefaultID();

private:
	Logic _logic = Logic(Logic::Type::ROOT_NONE);
	DurationModifier _durationModifier;
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
