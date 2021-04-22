#include "headers/macro-action-wait.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const int MacroActionWait::id = 1;

bool MacroActionWait::_registered = MacroActionFactory::Register(
	MacroActionWait::id,
	{MacroActionWait::Create, MacroActionWaitEdit::Create,
	 "AdvSceneSwitcher.action.wait"});

bool MacroActionWait::PerformAction()
{
	std::this_thread::sleep_for(
		std::chrono::milliseconds((long long)_duration * 1000));
	return true;
}

bool MacroActionWait::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_double(obj, "duration", _duration);
	return true;
}

bool MacroActionWait::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration = obs_data_get_double(obj, "duration");
	return true;
}

MacroActionWaitEdit::MacroActionWaitEdit(
	std::shared_ptr<MacroActionWait> entryData)
{
	_duration = new QDoubleSpinBox();

	QWidget::connect(_duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.macro.action.wait.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWaitEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_duration->setValue(_entryData->_duration);
}

void MacroActionWaitEdit::DurationChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = value;
}
