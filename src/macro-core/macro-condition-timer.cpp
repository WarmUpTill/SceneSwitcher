#include "macro-condition-edit.hpp"
#include "macro-condition-timer.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

const std::string MacroConditionTimer::id = "timer";

bool MacroConditionTimer::_registered = MacroConditionFactory::Register(
	MacroConditionTimer::id,
	{MacroConditionTimer::Create, MacroConditionTimerEdit::Create,
	 "AdvSceneSwitcher.condition.timer", false});

static std::map<TimerType, std::string> timerTypes = {
	{TimerType::FIXED, "AdvSceneSwitcher.condition.timer.type.fixed"},
	{TimerType::RANDOM, "AdvSceneSwitcher.condition.timer.type.random"},
};

bool MacroConditionTimer::CheckCondition()
{
	if (_paused) {
		return _remaining == 0.;
	}
	if (_duration.DurationReached()) {
		if (!_oneshot) {
			_duration.Reset();
			if (_type == TimerType::RANDOM) {
				SetRandomTimeRemaining();
			}
		}
		return true;
	}
	return false;
}

void MacroConditionTimer::SetRandomTimeRemaining()
{
	double min, max;
	if (_duration.Seconds() <= _duration2.Seconds()) {
		min = _duration.Seconds();
		max = _duration2.Seconds();
	} else {
		min = _duration2.Seconds();
		max = _duration.Seconds();
	}
	std::uniform_real_distribution<double> unif(min, max);

	double remainingTime = unif(_re);
	_duration.SetTimeRemaining(remainingTime);
}

bool MacroConditionTimer::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	_duration.Save(obj);
	_duration2.Save(obj, "duration2");
	if (_saveRemaining) {
		obs_data_set_double(obj, "remaining",
				    _paused ? _remaining
					    : _duration.TimeRemaining());
	} else {
		obs_data_set_double(obj, "remaining", _duration.Seconds());
	}
	obs_data_set_bool(obj, "saveRemaining", _saveRemaining);
	obs_data_set_bool(obj, "paused", _paused);
	obs_data_set_bool(obj, "oneshot", _oneshot);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroConditionTimer::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_type = static_cast<TimerType>(obs_data_get_int(obj, "type"));
	_duration.Load(obj);
	// TODO: remove this fallback
	if (obs_data_get_int(obj, "version") == 1) {
		_duration2.Load(obj, "duration2");
	} else {
		_duration2.Load(obj, "seconds2");
		_duration2.SetUnit(static_cast<Duration::Unit>(
			obs_data_get_int(obj, "displayUnit2")));
	}
	_remaining = obs_data_get_double(obj, "remaining");
	_paused = obs_data_get_bool(obj, "paused");
	_saveRemaining = obs_data_get_bool(obj, "saveRemaining");
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
	_remaining = _duration.Seconds();
	_duration.Reset();
	if (_type == TimerType::RANDOM) {
		SetRandomTimeRemaining();
	}
}

static inline void populateTimerTypeSelection(QComboBox *list)
{
	for (auto entry : timerTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionTimerEdit::MacroConditionTimerEdit(
	QWidget *parent, std::shared_ptr<MacroConditionTimer> entryData)
	: QWidget(parent)
{
	_timerTypes = new QComboBox();
	_duration = new DurationSelection();
	_duration2 = new DurationSelection();
	_autoReset = new QCheckBox();
	_saveRemaining = new QCheckBox();
	_pauseConinue = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.timer.pause"));
	_reset = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.timer.reset"));
	_remaining = new QLabel();

	populateTimerTypeSelection(_timerTypes);

	QWidget::connect(_timerTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TimerTypeChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_duration2, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(Duration2Changed(const Duration &)));
	QWidget::connect(_pauseConinue, SIGNAL(clicked()), this,
			 SLOT(PauseContinueClicked()));
	QWidget::connect(_reset, SIGNAL(clicked()), this, SLOT(ResetClicked()));
	QWidget::connect(_autoReset, SIGNAL(stateChanged(int)), this,
			 SLOT(AutoResetChanged(int)));
	QWidget::connect(_saveRemaining, SIGNAL(stateChanged(int)), this,
			 SLOT(SaveRemainingChanged(int)));

	_timerLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _timerTypes},
		{"{{duration}}", _duration},
		{"{{duration2}}", _duration2},
		{"{{autoReset}}", _autoReset},
		{"{{remaining}}", _remaining},
		{"{{pauseContinue}}", _pauseConinue},
		{"{{reset}}", _reset},
		{"{{saveRemaining}}", _saveRemaining},
	};
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.timer.entry.line1.fixed"),
		_timerLayout, widgetPlaceholders);
	auto line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.timer.entry.line2"),
		line2Layout, widgetPlaceholders);
	auto line3Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.timer.entry.line3"),
		line3Layout, widgetPlaceholders);

	auto *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_timerLayout);
	mainLayout->addLayout(line2Layout);
	mainLayout->addLayout(line3Layout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	connect(&timer, SIGNAL(timeout()), this, SLOT(UpdateTimeRemaining()));
	timer.start(1000);
}

void MacroConditionTimerEdit::TimerTypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<TimerType>(type);
	SetWidgetVisibility();
}

void MacroConditionTimerEdit::DurationChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = dur;
}

void MacroConditionTimerEdit::Duration2Changed(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration2 = dur;
}

void MacroConditionTimerEdit::SaveRemainingChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_saveRemaining = state;
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

	_timerTypes->setCurrentIndex(static_cast<int>(_entryData->_type));
	_duration->SetDuration(_entryData->_duration);
	_duration2->SetDuration(_entryData->_duration2);
	_autoReset->setChecked(!_entryData->_oneshot);
	_saveRemaining->setChecked(_entryData->_saveRemaining);
	SetPauseContinueButtonLabel();
	SetWidgetVisibility();
}

void MacroConditionTimerEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_timerLayout->removeWidget(_timerTypes);
	_timerLayout->removeWidget(_duration);
	_timerLayout->removeWidget(_duration2);

	clearLayout(_timerLayout);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{type}}", _timerTypes},
		{"{{duration}}", _duration},
		{"{{duration2}}", _duration2},
	};

	if (_entryData->_type == TimerType::RANDOM) {
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.condition.timer.entry.line1.random"),
			_timerLayout, widgetPlaceholders);
		_duration2->show();
	} else {
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.condition.timer.entry.line1.fixed"),
			_timerLayout, widgetPlaceholders);
		_duration2->hide();
	}
}

} // namespace advss
