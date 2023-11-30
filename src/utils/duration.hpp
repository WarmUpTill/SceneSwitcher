#pragma once
#include "variable-number.hpp"
#include "obs-data.h"

#include <chrono>

namespace advss {

class Duration {
public:
	Duration() = default;
	Duration(double initialValueInSeconds);

	void Save(obs_data_t *obj, const char *name = "duration") const;
	void Load(obs_data_t *obj, const char *name = "duration");

	bool DurationReached();
	bool IsReset() const;
	double Seconds() const;
	double Milliseconds() const;
	double TimeRemaining() const;
	void SetTimeRemaining(double);
	void Reset();
	std::string ToString() const;

	enum class Unit {
		SECONDS,
		MINUTES,
		HOURS,
	};
	Unit GetUnit() const { return _unit; }

	// TODO: Remove
	// Only use this function if you intend to convert old settings formats
	void SetUnit(Unit u);

private:
	NumberVariable<double> _value = 0.;
	Unit _unit = Unit::SECONDS;
	std::chrono::high_resolution_clock::time_point _startTime;

	friend class DurationSelection;
};

} // namespace advss
