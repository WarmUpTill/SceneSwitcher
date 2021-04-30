#include "headers/macro-action-wait.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <random>

const int MacroActionWait::id = 1;

bool MacroActionWait::_registered = MacroActionFactory::Register(
	MacroActionWait::id,
	{MacroActionWait::Create, MacroActionWaitEdit::Create,
	 "AdvSceneSwitcher.action.wait"});

static std::unordered_map<WaitType, std::string> waitTypes = {
	{WaitType::FIXED, "AdvSceneSwitcher.action.wait.type.fixed"},
	{WaitType::RANDOM, "AdvSceneSwitcher.action.wait.type.random"},
};

static std::random_device rd;
static std::default_random_engine re(rd());

bool MacroActionWait::PerformAction()
{
	double sleep_duration;
	if (_waitType == WaitType::FIXED) {
		sleep_duration = _duration;
	} else {
		double min = (_duration < _duration2) ? _duration : _duration2;
		double max = (_duration < _duration2) ? _duration2 : _duration;
		std::uniform_real_distribution<double> unif(min, max);
		sleep_duration = unif(re);
	}

	std::unique_lock<std::mutex> lock(switcher->m);
	switcher->cv.wait_for(lock, std::chrono::milliseconds((
					    long long)(sleep_duration * 1000)));
	return !switcher->stop;
}

bool MacroActionWait::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_double(obj, "duration", _duration);
	obs_data_set_double(obj, "duration2", _duration2);
	obs_data_set_int(obj, "waitType", static_cast<int>(_waitType));
	return true;
}

bool MacroActionWait::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration = obs_data_get_double(obj, "duration");
	_duration2 = obs_data_get_double(obj, "duration2");
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
	_duration = new QDoubleSpinBox();
	_duration2 = new QDoubleSpinBox();
	_waitType = new QComboBox();

	_duration->setMinimum(0.0);
	_duration->setMaximum(9999999.);
	_duration->setSuffix("s");

	_duration2->setMinimum(0.0);
	_duration2->setMaximum(9999999.);
	_duration2->setSuffix("s");

	populateTypeSelection(_waitType);

	QWidget::connect(_duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration2, SIGNAL(valueChanged(double)), this,
			 SLOT(Duration2Changed(double)));
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

	_duration->setValue(_entryData->_duration);
	_duration2->setValue(_entryData->_duration2);
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
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.macro.action.wait.entry.fixed"),
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
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.macro.action.wait.entry.random"),
		     _mainLayout, widgetPlaceholders);
	_duration2->show();
}

void MacroActionWaitEdit::Duration2Changed(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration2 = value;
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

void MacroActionWaitEdit::DurationChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration = value;
}
