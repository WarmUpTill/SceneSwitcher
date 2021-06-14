#include "headers/duration-control.hpp"
#include "headers/utility.hpp"
#include "obs-module.h"

#include <sstream>
#include <iomanip>
#include <QHBoxLayout>

void Duration::Save(obs_data_t *obj, const char *secondsName,
		    const char *unitName)
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
	if (_startTime.time_since_epoch().count() == 0) {
		_startTime = std::chrono::high_resolution_clock::now();
	}

	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);
	return runTime.count() >= seconds * 1000;
}

double Duration::TimeRemaining()
{
	if (_startTime.time_since_epoch().count() == 0) {
		return seconds;
	}
	auto runTime = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::high_resolution_clock::now() - _startTime);

	if (runTime.count() >= seconds * 1000) {
		return 0;
	}
	return (seconds * 1000 - runTime.count()) / 1000.;
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

std::string Duration::ToString()
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

DurationSelection::DurationSelection(QWidget *parent, bool showUnitSelection)
	: QWidget(parent), _unitMultiplier(1)
{
	_duration = new QDoubleSpinBox(parent);
	_duration->setMaximum(86400); // 24 hours

	_unitSelection = new QComboBox();
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

static void populateConditions(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.more"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.equal"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.duration.condition.less"));
}

DurationConstraintEdit::DurationConstraintEdit(QWidget *parent)
{
	_condition = new QComboBox(parent);
	_duration = new DurationSelection(parent);
	_toggle = new QPushButton(parent);
	_toggle->setMaximumSize(22, 22);
	_toggle->setIcon(
		QIcon(QString::fromStdString(getDataFilePath("res/time.svg"))));
	populateConditions(_condition);
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(_ConditionChanged(int)));
	QObject::connect(_duration, &DurationSelection::DurationChanged, this,
			 &DurationConstraintEdit::DurationChanged);
	QObject::connect(_duration, &DurationSelection::UnitChanged, this,
			 &DurationConstraintEdit::UnitChanged);
	QWidget::connect(_toggle, SIGNAL(clicked()), this,
			 SLOT(ToggleClicked()));

	QHBoxLayout *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(11);
	layout->addWidget(_toggle);
	layout->addWidget(_condition);
	layout->addWidget(_duration);
	setLayout(layout);
	Collapse(true);
}

void DurationConstraintEdit::SetValue(DurationConstraint &value)
{
	_duration->SetDuration(value.GetDuration());
	_condition->setCurrentIndex(static_cast<int>(value.GetCondition()));
	_duration->setVisible(value.GetCondition() != DurationCondition::NONE);
}

void DurationConstraintEdit::SetUnit(DurationUnit u)
{
	_duration->SetUnit(u);
}

void DurationConstraintEdit::SetDuration(const Duration &d)
{
	_duration->SetDuration(d);
}

void DurationConstraintEdit::_ConditionChanged(int value)
{
	auto cond = static_cast<DurationCondition>(value);
	Collapse(cond == DurationCondition::NONE);
	emit ConditionChanged(cond);
}

void DurationConstraintEdit::ToggleClicked()
{
	Collapse(false);
}

void DurationConstraintEdit::Collapse(bool collapse)
{
	_toggle->setVisible(collapse);
	_duration->setVisible(!collapse);
	_condition->setVisible(!collapse);
}
