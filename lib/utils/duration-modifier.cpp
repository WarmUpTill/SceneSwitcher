#include "duration-modifier.hpp"

namespace advss {

void DurationModifier::Save(obs_data_t *obj, const char *name,
			    const char *duration) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, name, static_cast<int>(_type));
	_duration.Save(data, duration);
	obs_data_set_obj(obj, "durationModifier", data);
	obs_data_release(data);
}

void DurationModifier::Load(obs_data_t *obj, const char *name,
			    const char *duration)
{
	obs_data_t *data = nullptr;
	if (obs_data_has_user_value(obj, "durationModifier")) {
		data = obs_data_get_obj(obj, "durationModifier");
	} else {
		// For backwards compatibility
		obs_data_addref(obj);
		data = obj;
	}

	// For backwards compatibility check if duration value exist without
	// time constraint condition - if so assume DurationCondition::MORE
	if (!obs_data_has_user_value(data, name) &&
	    obs_data_has_user_value(data, duration)) {
		obs_data_set_int(data, name, static_cast<int>(Type::MORE));
	}

	_type = static_cast<Type>(obs_data_get_int(data, name));
	_duration.Load(data, duration);

	// TODO: remove this fallback
	if (obs_data_has_user_value(data, "displayUnit")) {
		_duration.SetUnit(static_cast<Duration::Unit>(
			obs_data_get_int(data, "displayUnit")));
	}

	obs_data_release(data);
}

void DurationModifier::SetTimeRemaining(double seconds)
{
	_duration.SetTimeRemaining(seconds);
}

bool DurationModifier::DurationReached()
{
	switch (_type) {
	case DurationModifier::Type::NONE:
		return true;
	case DurationModifier::Type::MORE:
		return _duration.DurationReached();
	case DurationModifier::Type::EQUAL:
		if (_duration.DurationReached() && !_durationWasReached) {
			_durationWasReached = true;
			return true;
		}
		break;
	case DurationModifier::Type::LESS:
		return !_duration.DurationReached();
	case DurationModifier::Type::WITHIN:
		if (_duration.IsReset()) {
			return false;
		}
		return !_duration.DurationReached();
	default:
		break;
	}
	return false;
}

void DurationModifier::ResetDuration()
{
	_durationWasReached = false;
	_duration.Reset();
}

bool DurationModifier::CheckConditionWithDurationModifier(bool conditionValue)
{
	if (_type != DurationModifier::Type::WITHIN && !conditionValue) {
		ResetDuration();
	}
	if (_type == DurationModifier::Type::WITHIN && conditionValue) {
		ResetDuration();
	}

	switch (_type) {
	case DurationModifier::Type::NONE:
	case DurationModifier::Type::MORE:
	case DurationModifier::Type::EQUAL:
	case DurationModifier::Type::LESS:
		return conditionValue && DurationReached();
	case DurationModifier::Type::WITHIN:
		if (conditionValue) {
			SetTimeRemaining(_duration.Seconds());
		}
		return conditionValue || DurationReached();
	default:
		break;
	}

	return conditionValue;
}

} // namespace advss
