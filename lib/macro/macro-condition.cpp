#include "macro-condition.hpp"

namespace advss {

MacroCondition::MacroCondition(Macro *m, bool supportsVariableValue)
	: MacroSegment(m, supportsVariableValue)
{
}

bool MacroCondition::Save(obs_data_t *obj) const
{
	MacroSegment::Save(obj);
	obs_data_set_string(obj, "id", GetId().c_str());
	_logic.Save(obj, "logic");
	_durationModifier.Save(obj);
	return true;
}

bool MacroCondition::Load(obs_data_t *obj)
{
	MacroSegment::Load(obj);
	_logic.Load(obj, "logic");
	_durationModifier.Load(obj);
	return true;
}

void MacroCondition::ValidateLogicSelection(bool isRootCondition,
					    const char *context)
{
	if (_logic.IsValidSelection(isRootCondition)) {
		return;
	}

	if (_logic.IsRootType()) {
		_logic.SetType(Logic::Type::ROOT_NONE);
		blog(LOG_WARNING,
		     "setting invalid logic selection to 'if' for macro %s",
		     context);
		return;
	}

	_logic.SetType(Logic::Type::NONE);
	blog(LOG_WARNING,
	     "setting invalid logic selection to 'ignore' for macro %s",
	     context);
}

void MacroCondition::ResetDuration()
{
	_durationModifier.ResetDuration();
}

bool MacroCondition::CheckDurationModifier(bool conditionValue)
{
	return _durationModifier.CheckConditionWithDurationModifier(
		conditionValue);
}

DurationModifier MacroCondition::GetDurationModifier() const
{
	return _durationModifier;
}

void MacroCondition::SetDurationModifier(DurationModifier::Type m)
{
	_durationModifier.SetModifier(m);
}

void MacroCondition::SetDuration(const Duration &duration)
{
	_durationModifier.SetDuration(duration);
}

std::string_view MacroCondition::GetDefaultID()
{
	return "scene";
}

MacroRefCondition::MacroRefCondition(Macro *m, bool supportsVariableValue)
	: MacroCondition(m, supportsVariableValue)
{
}

bool MacroRefCondition::PostLoad()
{
	_macro.PostLoad();
	return true;
}

MultiMacroRefCondition::MultiMacroRefCondition(Macro *m,
					       bool supportsVariableValue)
	: MacroCondition(m, supportsVariableValue)
{
}

bool MultiMacroRefCondition::PostLoad()
{
	for (auto &macro : _macros) {
		macro.PostLoad();
	}
	return true;
}

} // namespace advss
