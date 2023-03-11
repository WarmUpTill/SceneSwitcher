#include "duration-control.hpp"
#include "mouse-wheel-guard.hpp"
#include "obs-module.h"
#include "utility.hpp"

#include <sstream>
#include <iomanip>
#include <QHBoxLayout>

Duration::Duration(double initialValueInSeconds) : _value(initialValueInSeconds)
{
}

void Duration::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	_value.Save(data, "value");
	obs_data_set_int(data, "unit", static_cast<int>(_unit));
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

static int durationUnitToMultiplier(Duration::Unit u)
{
	switch (u) {
	case Duration::Unit::SECONDS:
		return 1;
	case Duration::Unit::MINUTES:
		return 60;
	case Duration::Unit::HOURS:
		return 3600;
	default:
		break;
	}

	return 0;
}

void Duration::Load(obs_data_t *obj, const char *name)
{
	auto data = obs_data_get_obj(obj, name);

	// TODO: remove this fallback
	if (!data || !obs_data_has_user_value(data, "version") ||
	    obs_data_get_int(data, "version") != 1) {
		bool usingDefaultArgs = strcmp("duration", name) == 0;
		if (usingDefaultArgs) {
			_value = obs_data_get_double(obj, "seconds");
		}
		if (_value.GetValue() == 0.0) {
			_value = obs_data_get_double(obj, name);
		}

		if (usingDefaultArgs) {
			_unit = static_cast<Duration::Unit>(
				obs_data_get_int(obj, "displayUnit"));
		} else if (_value.GetValue() >= 86400) {
			_unit = (_value.GetValue() / 60 >= 86400)
					? Duration::Unit::HOURS
					: Duration::Unit::MINUTES;
		} else {
			_unit = Duration::Unit::SECONDS;
		}

		_value = _value / durationUnitToMultiplier(_unit);

		obs_data_release(data);
		return;
	}

	_value.Load(data, "value");
	_unit = static_cast<Duration::Unit>(obs_data_get_int(data, "unit"));
	obs_data_release(data);
}

bool Duration::DurationReached()
{
	if (IsReset()) {
		_startTime = std::chrono::high_resolution_clock::now();
	}

	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);
	return runTime.count() >= Milliseconds();
}

bool Duration::IsReset() const
{
	return _startTime.time_since_epoch().count() == 0;
}

double Duration::Seconds() const
{
	return _value.GetValue() * durationUnitToMultiplier(_unit);
}

double Duration::Milliseconds() const
{
	return Seconds() * 1000.0;
}

double Duration::TimeRemaining() const
{
	if (IsReset()) {
		return Seconds();
	}
	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);

	if (runTime.count() >= Milliseconds()) {
		return 0;
	}
	return (Milliseconds() - runTime.count()) / 1000.0;
}

void Duration::SetTimeRemaining(double remaining)
{
	long long msPassed = (Seconds() - remaining) * 1000;
	_startTime = std::chrono::high_resolution_clock::now() -
		     std::chrono::milliseconds(msPassed);
}

void Duration::Reset()
{
	_startTime = {};
}

static std::string durationUnitToString(Duration::Unit u)
{
	switch (u) {
	case Duration::Unit::SECONDS:
		return obs_module_text("AdvSceneSwitcher.unit.secends");
	case Duration::Unit::MINUTES:
		return obs_module_text("AdvSceneSwitcher.unit.minutes");
	case Duration::Unit::HOURS:
		return obs_module_text("AdvSceneSwitcher.unit.hours");
	default:
		break;
	}

	return "";
}

std::string Duration::ToString() const
{
	std::ostringstream ss;
	ss << std::fixed << std::setprecision(2) << _value.GetValue() << " "
	   << durationUnitToString(_unit);
	if (!_value.IsFixedType()) {
		ss << " [" << GetWeakVariableName(_value.GetVariable()) << "]";
	}
	return ss.str();
}

void Duration::SetUnit(Unit u)
{
	double prevMultiplier = durationUnitToMultiplier(_unit);
	double newMultiplier = durationUnitToMultiplier(u);
	_unit = u;
	_value = _value * (prevMultiplier / newMultiplier);
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
	  _duration(new VariableDoubleSpinBox(parent)),
	  _unitSelection(new QComboBox())
{
	_duration->setMinimum(minValue);
	_duration->setMaximum(86400); // 24 hours
	PreventMouseWheelAdjustWithoutFocus(_duration);

	populateUnits(_unitSelection);

	QWidget::connect(
		_duration,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(_DurationChanged(const NumberVariable<double> &)));

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

void DurationSelection::SetDuration(const Duration &d)
{
	_current = d;
	_duration->SetValue(d._value);
	const QSignalBlocker b(_unitSelection);
	_unitSelection->setCurrentIndex(static_cast<int>(d._unit));
}

void DurationSelection::_DurationChanged(const NumberVariable<double> &value)
{
	_current._value = value;
	emit DurationChanged(_current);
}

void DurationSelection::_UnitChanged(int idx)
{
	Duration::Unit unit = static_cast<Duration::Unit>(idx);
	double prevMultiplier = durationUnitToMultiplier(_current._unit);
	double newMultiplier = durationUnitToMultiplier(unit);
	_current._unit = unit;
	const QSignalBlocker b(_duration);
	_duration->SetFixedValue(_duration->Value() *
				 (prevMultiplier / newMultiplier));
	emit DurationChanged(_current);
}
