#include "duration-control.hpp"
#include "mouse-wheel-guard.hpp"
#include "obs-module-helper.hpp"

#include <QHBoxLayout>

namespace advss {

static void populateUnits(QComboBox *list)
{
	list->addItems({obs_module_text("AdvSceneSwitcher.unit.seconds"),
			obs_module_text("AdvSceneSwitcher.unit.minutes"),
			obs_module_text("AdvSceneSwitcher.unit.hours"),
			obs_module_text("AdvSceneSwitcher.unit.days"),
			obs_module_text("AdvSceneSwitcher.unit.weeks"),
			obs_module_text("AdvSceneSwitcher.unit.months"),
			obs_module_text("AdvSceneSwitcher.unit.years")});
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
	double prevMultiplier =
		Duration::ConvertUnitToMultiplier(_current._unit);
	double newMultiplier = Duration::ConvertUnitToMultiplier(unit);
	_current._unit = unit;
	_duration->SetFixedValue(_duration->Value() *
				 (prevMultiplier / newMultiplier));
	emit DurationChanged(_current);
}

} // namespace advss
