#include "headers/macro-condition.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
	{LogicType::NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::AND, {"AdvSceneSwitcher.logic.and"}},
	{LogicType::OR, {"AdvSceneSwitcher.logic.or"}},
	{LogicType::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{LogicType::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
	{LogicType::ROOT_NONE, {"AdvSceneSwitcher.logic.rootNone"}},
	{LogicType::ROOT_NOT, {"AdvSceneSwitcher.logic.not"}},
};

void DurationConstraint::Save(obs_data_t *obj, const char *condName,
			      const char *secondsName, const char *unitName)
{
	obs_data_set_int(obj, condName, static_cast<int>(_type));
	_dur.Save(obj, secondsName, unitName);
}

void DurationConstraint::Load(obs_data_t *obj, const char *condName,
			      const char *secondsName, const char *unitName)
{
	// For backwards compatability check if duration value exist without
	// time constraint condition - if so assume DurationCondition::MORE
	if (!obs_data_has_user_value(obj, condName) &&
	    obs_data_has_user_value(obj, secondsName)) {
		obs_data_set_int(obj, condName,
				 static_cast<int>(DurationCondition::MORE));
	}

	_type = static_cast<DurationCondition>(obs_data_get_int(obj, condName));
	_dur.Load(obj, secondsName, unitName);
}

bool DurationConstraint::DurationReached()
{
	switch (_type) {
	case DurationCondition::NONE:
		return true;
		break;
	case DurationCondition::MORE:
		return _dur.DurationReached();
		break;
	case DurationCondition::EQUAL:
		if (_dur.DurationReached() && !_timeReached) {
			_timeReached = true;
			return true;
		}
		break;
	case DurationCondition::LESS:
		return !_dur.DurationReached();
		break;
	default:
		break;
	}
	return false;
}

void DurationConstraint::Reset()
{
	_timeReached = false;
	_dur.Reset();
}

bool MacroCondition::Save(obs_data_t *obj)
{
	MacroSegment::Save(obj);
	obs_data_set_string(obj, "id", GetId().c_str());
	obs_data_set_int(obj, "logic", static_cast<int>(_logic));
	_duration.Save(obj);
	return true;
}

bool MacroCondition::Load(obs_data_t *obj)
{
	MacroSegment::Load(obj);
	_logic = static_cast<LogicType>(obs_data_get_int(obj, "logic"));
	_duration.Load(obj);
	return true;
}

void MacroCondition::SetDurationConstraint(const DurationConstraint &dur)
{
	_duration = dur;
}

void MacroCondition::SetDurationCondition(DurationCondition cond)
{
	_duration.SetCondition(cond);
}

void MacroCondition::SetDurationUnit(DurationUnit u)
{
	_duration.SetUnit(u);
}

void MacroCondition::SetDuration(double seconds)
{
	_duration.SetValue(seconds);
}

void MacroRefCondition::ResolveMacroRef()
{
	_macro.UpdateRef();
}
