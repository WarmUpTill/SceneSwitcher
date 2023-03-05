#include "duration-control.hpp"
#include "mouse-wheel-guard.hpp"
#include "obs-module.h"
#include "utility.hpp"

#include <sstream>
#include <iomanip>
#include <QHBoxLayout>

Duration::Duration(double initialValueInSeconds)
	: seconds(initialValueInSeconds)
{
}

void Duration::Save(obs_data_t *obj, const char *secondsName,
		    const char *unitName) const
{
	obs_data_set_double(obj, secondsName, seconds);
	obs_data_set_int(obj, unitName, static_cast<int>(displayUnit));
}

void Duration::Load(obs_data_t *obj, const char *secondsName,
		    const char *unitName)
{
	seconds = obs_data_get_double(obj, secondsName);
	displayUnit =
		static_cast<DurationUnit>(obs_data_get_int(obj, unitName));
}

bool Duration::DurationReached()
{
	if (IsReset()) {
		_startTime = std::chrono::high_resolution_clock::now();
	}

	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);
	return runTime.count() >= seconds * 1000;
}

bool Duration::IsReset() const
{
	return _startTime.time_since_epoch().count() == 0;
}

double Duration::TimeRemaining() const
{
	if (IsReset()) {
		return seconds;
	}
	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);

	if (runTime.count() >= seconds * 1000) {
		return 0;
	}
	return (seconds * 1000 - runTime.count()) / 1000.;
}

void Duration::SetTimeRemaining(double remaining)
{
	long long msPassed = (seconds - remaining) * 1000;
	_startTime = std::chrono::high_resolution_clock::now() -
		     std::chrono::milliseconds(msPassed);
}

void Duration::Reset()
{
	_startTime = {};
}

int durationUnitToMultiplier(DurationUnit u)
{
	switch (u) {
	case DurationUnit::SECONDS:
		return 1;
	case DurationUnit::MINUTES:
		return 60;
	case DurationUnit::HOURS:
		return 3600;
	default:
		break;
	}

	return 0;
}

std::string durationUnitToString(DurationUnit u)
{
	switch (u) {
	case DurationUnit::SECONDS:
		return obs_module_text("AdvSceneSwitcher.unit.secends");
	case DurationUnit::MINUTES:
		return obs_module_text("AdvSceneSwitcher.unit.minutes");
	case DurationUnit::HOURS:
		return obs_module_text("AdvSceneSwitcher.unit.hours");
	default:
		break;
	}

	return "";
}

std::string Duration::ToString() const
{
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(2)
	   << seconds / durationUnitToMultiplier(displayUnit) << " "
	   << durationUnitToString(displayUnit);
	return ss.str();
}

static void populateUnits(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.secends"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.minutes"));
	list->addItem(obs_module_text("AdvSceneSwitcher.unit.hours"));
}

DurationSelection::DurationSelection(QWidget *parent, bool showUnitSelection,
				     double minValue)
	: QWidget(parent),
	  _duration(new QDoubleSpinBox(parent)),
	  _unitSelection(new QComboBox()),
	  _unitMultiplier(1)
{
	_duration->setMinimum(minValue);
	_duration->setMaximum(86400); // 24 hours
	PreventMouseWheelAdjustWithoutFocus(_duration);

	populateUnits(_unitSelection);

	QWidget::connect(_duration, SIGNAL(valueChanged(double)), this,
			 SLOT(_DurationChanged(double)));
	QWidget::connect(_unitSelection, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(_UnitChanged(int)));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(11);
	layout->addWidget(_duration);
	if (showUnitSelection) {
		layout->addWidget(_unitSelection);
	}
	setLayout(layout);
}

void DurationSelection::SetValue(double value)
{
	_duration->setValue(value / _unitMultiplier);
}

void DurationSelection::SetUnit(DurationUnit u)
{
	_unitSelection->setCurrentIndex(static_cast<int>(u));
}

void DurationSelection::SetDuration(Duration d)
{
	SetUnit(d.displayUnit);
	SetValue(d.seconds);
}

void DurationSelection::_DurationChanged(double value)
{
	emit DurationChanged(value * _unitMultiplier);
}

void DurationSelection::_UnitChanged(int idx)
{
	DurationUnit unit = static_cast<DurationUnit>(idx);
	double prevMultiplier = _unitMultiplier;
	_unitMultiplier = durationUnitToMultiplier(unit);
	_duration->setValue(_duration->value() *
			    (prevMultiplier / _unitMultiplier));

	emit UnitChanged(unit);
}
