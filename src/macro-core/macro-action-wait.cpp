#include "macro-action-wait.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

#include <random>

const std::string MacroActionWait::id = "wait";

bool MacroActionWait::_registered = MacroActionFactory::Register(
	MacroActionWait::id,
	{MacroActionWait::Create, MacroActionWaitEdit::Create,
	 "AdvSceneSwitcher.action.wait"});

static std::map<WaitType, std::string> waitTypes = {
	{WaitType::FIXED, "AdvSceneSwitcher.action.wait.type.fixed"},
	{WaitType::RANDOM, "AdvSceneSwitcher.action.wait.type.random"},
};

static std::random_device rd;
static std::default_random_engine re(rd());

bool MacroActionWait::PerformAction()
{
	double sleepDuration;
	if (_waitType == WaitType::FIXED) {
		sleepDuration = _duration.seconds;
	} else {
		double min = (_duration.seconds < _duration2.seconds)
				     ? _duration.seconds
				     : _duration2.seconds;
		double max = (_duration.seconds < _duration2.seconds)
				     ? _duration2.seconds
				     : _duration.seconds;
		std::uniform_real_distribution<double> unif(min, max);
		sleepDuration = unif(re);
	}
	vblog(LOG_INFO, "perform action wait with duration of %f",
	      sleepDuration);

	auto time = std::chrono::high_resolution_clock::now() +
		    std::chrono::milliseconds((int)(sleepDuration * 1000));
	auto macro = GetMacro();
	switcher->abortMacroWait = false;
	std::unique_lock<std::mutex> lock(switcher->m);
	while (!switcher->abortMacroWait && !macro->GetStop()) {
		if (switcher->macroWaitCv.wait_until(lock, time) ==
		    std::cv_status::timeout) {
			break;
		}
	}

	return !switcher->abortMacroWait;
}

bool MacroActionWait::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_duration.Save(obj);
	_duration2.Save(obj, "seconds2", "displayUnit2");
	obs_data_set_int(obj, "waitType", static_cast<int>(_waitType));
	return true;
}

bool MacroActionWait::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration.Load(obj);
	_duration2.Load(obj, "seconds2", "displayUnit2");
	_waitType = static_cast<WaitType>(obs_data_get_int(obj, "waitType"));
	return true;
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : waitTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionWaitEdit::MacroActionWaitEdit(
	QWidget *parent, std::shared_ptr<MacroActionWait> entryData)
	: QWidget(parent)
{
	_duration = new DurationSelection();
	_duration2 = new DurationSelection();
	_waitType = new QComboBox();

	populateTypeSelection(_waitType);

	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_duration2, SIGNAL(DurationChanged(double)), this,
			 SLOT(Duration2Changed(double)));
	QWidget::connect(_duration2, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(Duration2UnitChanged(DurationUnit)));
	QWidget::connect(_waitType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));

	_mainLayout = new QHBoxLayout;
	setLayout(_mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionWaitEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	if (_entryData->_waitType == WaitType::FIXED) {
		SetupFixedDurationEdit();
	} else {
		SetupRandomDurationEdit();
	}

	_duration->SetDuration(_entryData->_duration);
	_duration2->SetDuration(_entryData->_duration2);
	_waitType->setCurrentIndex(static_cast<int>(_entryData->_waitType));
}

void MacroActionWaitEdit::SetupFixedDurationEdit()
{
	_mainLayout->removeWidget(_duration);
	_mainLayout->removeWidget(_duration2);
	_mainLayout->removeWidget(_waitType);
	clearLayout(_mainLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
		{"{{waitType}}", _waitType},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.wait.entry.fixed"),
		_mainLayout, widgetPlaceholders);
	_duration2->hide();
}

void MacroActionWaitEdit::SetupRandomDurationEdit()
{
	_mainLayout->removeWidget(_duration);
	_mainLayout->removeWidget(_duration2);
	_mainLayout->removeWidget(_waitType);
	clearLayout(_mainLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{duration}}", _duration},
		{"{{duration2}}", _duration2},
		{"{{waitType}}", _waitType},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.wait.entry.random"),
		_mainLayout, widgetPlaceholders);
	_duration2->show();
}

void MacroActionWaitEdit::TypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	WaitType type = static_cast<WaitType>(value);

	if (type == WaitType::FIXED) {
		SetupFixedDurationEdit();
	} else {
		SetupRandomDurationEdit();
	}

	_entryData->_waitType = type;
}

void MacroActionWaitEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroActionWaitEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroActionWaitEdit::Duration2Changed(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration2.seconds = seconds;
}

void MacroActionWaitEdit::Duration2UnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration2.displayUnit = unit;
}
