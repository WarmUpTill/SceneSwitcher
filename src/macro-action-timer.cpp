#include "headers/macro-action-timer.hpp"
#include "headers/macro-condition-timer.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <random>

const std::string MacroActionTimer::id = "timer";

bool MacroActionTimer::_registered = MacroActionFactory::Register(
	MacroActionTimer::id,
	{MacroActionTimer::Create, MacroActionTimerEdit::Create,
	 "AdvSceneSwitcher.action.Timer"});

static std::map<TimerAction, std::string> timerActions = {
	{TimerAction::PAUSE, "AdvSceneSwitcher.action.timer.type.pause"},
	{TimerAction::CONTINUE, "AdvSceneSwitcher.action.timer.type.continue"},
	{TimerAction::RESET, "AdvSceneSwitcher.action.timer.type.reset"},
	{TimerAction::SET_TIME_REMAINING,
	 "AdvSceneSwitcher.action.timer.type.setTimeRemaining"},
};

bool MacroActionTimer::PerformAction()
{
	if (!_macro.get()) {
		return true;
	}
	for (auto c : _macro->Conditions()) {
		if (c->GetId() != "timer") {
			continue;
		}
		auto timerCondition =
			dynamic_cast<MacroConditionTimer *>(c.get());
		if (!timerCondition) {
			continue;
		}

		switch (_actionType) {
		case TimerAction::PAUSE:
			timerCondition->Pause();
			break;
		case TimerAction::CONTINUE:
			timerCondition->Continue();
			break;
		case TimerAction::RESET:
			timerCondition->Reset();
			break;
		case TimerAction::SET_TIME_REMAINING:
			timerCondition->_duration.SetTimeRemaining(
				_duration.seconds);
			break;
		default:
			break;
		}
	}
	return true;
}

void MacroActionTimer::LogAction()
{
	if (!_macro.get()) {
		return;
	}
	switch (_actionType) {
	case TimerAction::PAUSE:
		vblog(LOG_INFO, "paused timers on \"%s\"",
		      _macro->Name().c_str());
		break;
	case TimerAction::CONTINUE:
		vblog(LOG_INFO, "continued timers on \"%s\"",
		      _macro->Name().c_str());
		break;
	case TimerAction::RESET:
		vblog(LOG_INFO, "reset timers on \"%s\"",
		      _macro->Name().c_str());
		break;
	case TimerAction::SET_TIME_REMAINING:
		vblog(LOG_INFO,
		      "set time remaining of timers on \"%s\" to \"%s\"",
		      _macro->Name().c_str(), _duration.ToString().c_str());
		break;
	default:
		break;
	}
}

bool MacroActionTimer::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_macro.Save(obj);
	_duration.Save(obj);
	obs_data_set_int(obj, "actionType", static_cast<int>(_actionType));
	return true;
}

bool MacroActionTimer::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_macro.Load(obj);
	_duration.Load(obj);
	_actionType =
		static_cast<TimerAction>(obs_data_get_int(obj, "actionType"));
	return true;
}

std::string MacroActionTimer::GetShortDesc()
{
	if (_macro.get()) {
		return _macro->Name();
	}
	return "";
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : timerActions) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionTimerEdit::MacroActionTimerEdit(
	QWidget *parent, std::shared_ptr<MacroActionTimer> entryData)
	: QWidget(parent)
{
	_macros = new MacroSelection(parent);
	_duration = new DurationSelection();
	_timerAction = new QComboBox();

	populateTypeSelection(_timerAction);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));
	QWidget::connect(_timerAction, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionTypeChanged(int)));

	_mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macros}}", _macros},
		{"{{duration}}", _duration},
		{"{{timerAction}}", _timerAction},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.timer.entry"),
		     _mainLayout, widgetPlaceholders);
	setLayout(_mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTimerEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_macros->SetCurrentMacro(_entryData->_macro.get());
	_duration->SetDuration(_entryData->_duration);
	_timerAction->setCurrentIndex(
		static_cast<int>(_entryData->_actionType));
	SetWidgetVisibility();
}

void MacroActionTimerEdit::ActionTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_actionType = static_cast<TimerAction>(value);
	SetWidgetVisibility();
}

void MacroActionTimerEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_duration->setVisible(_entryData->_actionType ==
			      TimerAction::SET_TIME_REMAINING);
	adjustSize();
}

void MacroActionTimerEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroActionTimerEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroActionTimerEdit::MacroChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_macro.UpdateRef(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionTimerEdit::MacroRemove(const QString &)
{
	if (_entryData) {
		_entryData->_macro.UpdateRef();
	}
}
