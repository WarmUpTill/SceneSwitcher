#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-idle.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionIdle::id = "idle";

bool MacroConditionIdle::_registered = MacroConditionFactory::Register(
	MacroConditionIdle::id,
	{MacroConditionIdle::Create, MacroConditionIdleEdit::Create,
	 "AdvSceneSwitcher.condition.idle", false});

bool MacroConditionIdle::CheckCondition()
{
	return secondsSinceLastInput() >= _duration.seconds;
}

bool MacroConditionIdle::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_duration.Save(obj);
	return true;
}

bool MacroConditionIdle::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_duration.Load(obj);
	return true;
}

MacroConditionIdleEdit::MacroConditionIdleEdit(
	QWidget *parent, std::shared_ptr<MacroConditionIdle> entryData)
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
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.idle.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionIdleEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionIdleEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionIdleEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_duration->SetDuration(_entryData->_duration);
}
