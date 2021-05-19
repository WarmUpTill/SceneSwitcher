#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-interval.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionInterval::id = 12;

bool MacroConditionInterval::_registered = MacroConditionFactory::Register(
	MacroConditionInterval::id,
	{MacroConditionInterval::Create, MacroConditionIntervalEdit::Create,
	 "AdvSceneSwitcher.condition.interval"});

bool MacroConditionInterval::CheckCondition()
{
	if (_duration.DurationReached()) {
		_duration.Reset();
		return true;
	}
	return false;
}

bool MacroConditionInterval::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_duration.Save(obj);
	return true;
}

bool MacroConditionInterval::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_duration.Load(obj);
	return true;
}

MacroConditionIntervalEdit::MacroConditionIntervalEdit(
	QWidget *parent, std::shared_ptr<MacroConditionInterval> entryData)
	: QWidget(parent)
{
	_duration = new DurationSelection();

	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.interval.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionIntervalEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionIntervalEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionIntervalEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_duration->SetDuration(_entryData->_duration);
}
