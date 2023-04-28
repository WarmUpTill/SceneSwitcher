#include "macro-condition-edit.hpp"
#include "macro-condition-idle.hpp"
#include "platform-funcs.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

const std::string MacroConditionIdle::id = "idle";

bool MacroConditionIdle::_registered = MacroConditionFactory::Register(
	MacroConditionIdle::id,
	{MacroConditionIdle::Create, MacroConditionIdleEdit::Create,
	 "AdvSceneSwitcher.condition.idle", false});

bool MacroConditionIdle::CheckCondition()
{
	auto seconds = secondsSinceLastInput();
	SetVariableValue(std::to_string(seconds));
	return seconds >= _duration.Seconds();
}

bool MacroConditionIdle::Save(obs_data_t *obj) const
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

	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.idle.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionIdleEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = dur;
}

void MacroConditionIdleEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_duration->SetDuration(_entryData->_duration);
}

} // namespace advss
