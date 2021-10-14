#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-timer.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionTimer::id = "timer";

bool MacroConditionTimer::_registered = MacroConditionFactory::Register(
	MacroConditionTimer::id,
	{MacroConditionTimer::Create, MacroConditionTimerEdit::Create,
	 "AdvSceneSwitcher.condition.timer", false});

bool MacroConditionTimer::CheckCondition()
{
	if (_paused) {
		return _remaining == 0.;
	}
	if (_duration.DurationReached()) {
		if (!_oneshot) {
			_duration.Reset();
		}
		return true;
	}
	return false;
}

bool MacroConditionTimer::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_duration.Save(obj);
	if (!_paused) {
		_remaining = _duration.TimeRemaining();
	}
	obs_data_set_double(obj, "remaining", _remaining);
	obs_data_set_bool(obj, "paused", _paused);
	obs_data_set_bool(obj, "oneshot", _oneshot);
	return true;
}

bool MacroConditionTimer::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_duration.Load(obj);
	_remaining = obs_data_get_double(obj, "remaining");
	_paused = obs_data_get_bool(obj, "paused");
	if (!obs_data_has_user_value(obj, "oneshot")) {
		_oneshot = false;
	} else {
		_oneshot = obs_data_get_bool(obj, "oneshot");
	}
	_duration.SetTimeRemaining(_remaining);
	return true;
}

void MacroConditionTimer::Pause()
{
	if (!_paused) {
		_paused = true;
		_remaining = _duration.TimeRemaining();
	}
}

void MacroConditionTimer::Continue()
{
	if (_paused) {
		_paused = false;
		_duration.SetTimeRemaining(_remaining);
	}
}

void MacroConditionTimer::Reset()
{
	_remaining = _duration.seconds;
	_duration.Reset();
}

MacroConditionTimerEdit::MacroConditionTimerEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTimer> entryData)
	: QWidget(parent)
{
	_duration = new DurationSelection();
	_autoReset = new QCheckBox();
	_pauseConinue = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.timer.pause"));
	_reset = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.timer.reset"));
	_remaining = new QLabel();

	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_pauseConinue, SIGNAL(clicked()), this,
			 SLOT(PauseContinueClicked()));
	QWidget::connect(_reset, SIGNAL(clicked()), this, SLOT(ResetClicked()));
	QWidget::connect(_autoReset, SIGNAL(stateChanged(int)), this,
			 SLOT(AutoResetChanged(int)));

	auto line1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
		{"{{autoReset}}", _autoReset},
		{"{{remaining}}", _remaining},
		{"{{pauseContinue}}", _pauseConinue},
		{"{{reset}}", _reset},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.timer.entry.line1"),
		line1Layout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.timer.entry.line2"),
		line2Layout, widgetPlaceholders);
	auto line3Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.timer.entry.line3"),
		line3Layout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	connect(&timer, SIGNAL(timeout()), this, SLOT(UpdateTimeRemaining()));
	timer.start(1000);
}

void MacroConditionTimerEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionTimerEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionTimerEdit::AutoResetChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_oneshot = !state;
}

void MacroConditionTimerEdit::PauseContinueClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (_entryData->_paused) {
		timer.start(1000);
		_entryData->Continue();
	} else {
		_entryData->Pause();
		timer.stop();
	}
	SetPauseContinueButtonLabel();
}

void MacroConditionTimerEdit::ResetClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->Reset();
}

void MacroConditionTimerEdit::UpdateTimeRemaining()
{
	if (!_entryData) {
		_remaining->setText("-");
		return;
	}

	if (_entryData->_paused) {
		_remaining->setText(QString::number(_entryData->_remaining));
	} else {
		_remaining->setText(
			QString::number(_entryData->_duration.TimeRemaining()));
	}
}

void MacroConditionTimerEdit::SetPauseContinueButtonLabel()
{
	if (!_entryData) {
		return;
	}

	if (_entryData->_paused) {
		_pauseConinue->setText(obs_module_text(
			"AdvSceneSwitcher.condition.timer.continue"));
	} else {
		_pauseConinue->setText(obs_module_text(
			"AdvSceneSwitcher.condition.timer.pause"));
	}
}

void MacroConditionTimerEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_duration->SetDuration(_entryData->_duration);
	_autoReset->setChecked(!_entryData->_oneshot);
	SetPauseContinueButtonLabel();
}
