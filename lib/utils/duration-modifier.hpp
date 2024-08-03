#pragma once
#include "duration.hpp"

namespace advss {

class DurationModifier {
public:
	enum class Type { NONE, MORE, EQUAL, LESS, WITHIN };

	void Save(obs_data_t *obj, const char *name = "time_constraint",
		  const char *duration = "seconds") const;
	void Load(obs_data_t *obj, const char *name = "time_constraint",
		  const char *duration = "seconds");
	void SetModifier(Type type) { _type = type; }
	void SetDuration(const Duration &duration) { _duration = duration; }
	Type GetType() const { return _type; }
	Duration GetDuration() const { return _duration; }
	void ResetDuration();

	bool CheckConditionWithDurationModifier(bool conditionValue);

private:
	void SetTimeRemaining(double seconds);
	bool DurationReached();

	Type _type = Type::NONE;
	Duration _duration;
	bool _durationWasReached = false;
};

} // namespace advss
