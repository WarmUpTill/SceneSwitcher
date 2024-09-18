#include "time-helpers.hpp"
#include "obs-module-helper.hpp"

namespace advss {

static QString combineRelativeTimeParts(const double unitCount,
					const char *unitName)
{
	QString combined = obs_module_text("AdvSceneSwitcher.time.relative");

	return combined.arg(QString::number(unitCount, 'g', 2)).arg(unitName);
}

QString FormatRelativeTime(const double seconds)
{
	// 365.2425 days
	double divided = seconds / 31556952;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided,
			obs_module_text("AdvSceneSwitcher.unit.years"));
	}

	// 30.436875 days
	divided = seconds / 2629746;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided,
			obs_module_text("AdvSceneSwitcher.unit.months"));
	}

	divided = seconds / 604800;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided,
			obs_module_text("AdvSceneSwitcher.unit.weeks"));
	}

	divided = seconds / 86400;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided, obs_module_text("AdvSceneSwitcher.unit.days"));
	}

	divided = seconds / 3600;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided,
			obs_module_text("AdvSceneSwitcher.unit.hours"));
	}

	divided = seconds / 60;
	if (divided >= 1) {
		return combineRelativeTimeParts(
			divided,
			obs_module_text("AdvSceneSwitcher.unit.minutes"));
	}

	return combineRelativeTimeParts(
		seconds, obs_module_text("AdvSceneSwitcher.unit.seconds"));
}

} // namespace advss
