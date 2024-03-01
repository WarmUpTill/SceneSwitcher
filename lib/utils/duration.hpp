#pragma once
#include "variable-number.hpp"
#include "obs-data.h"

#include <chrono>

namespace advss {

class Duration {
public:
	EXPORT Duration() = default;
	EXPORT Duration(double initialValueInSeconds);

	EXPORT void Save(obs_data_t *obj, const char *name = "duration") const;
	EXPORT void Load(obs_data_t *obj, const char *name = "duration");

	EXPORT bool DurationReached();
	EXPORT bool IsReset() const;
	EXPORT double Seconds() const;
	EXPORT double Milliseconds() const;
	EXPORT double TimeRemaining() const;
	EXPORT void SetTimeRemaining(double);
	EXPORT void Reset();
	EXPORT std::string ToString() const;

	enum class Unit {
		SECONDS,
		MINUTES,
		HOURS,
	};
	EXPORT Unit GetUnit() const { return _unit; }

	// TODO: Remove
	// Only use this function if you intend to convert old settings formats
	EXPORT void SetUnit(Unit u);

	EXPORT void ResolveVariables();

private:
	DoubleVariable _value = 0.;
	Unit _unit = Unit::SECONDS;
	std::chrono::high_resolution_clock::time_point _startTime;

	friend class DurationSelection;
};

} // namespace advss
