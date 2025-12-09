#include "macro-action-timer.hpp"
#include "layout-helpers.hpp"
#include "macro-condition-timer.hpp"
#include "macro-helpers.hpp"

#include <random>

namespace advss {

const std::string MacroActionTimer::id = "timer";

bool MacroActionTimer::_registered = MacroActionFactory::Register(
	MacroActionTimer::id,
	{MacroActionTimer::Create, MacroActionTimerEdit::Create,
	 "AdvSceneSwitcher.action.timer"});

const static std::map<MacroActionTimer::Action, std::string> timerActions = {
	{MacroActionTimer::Action::PAUSE,
	 "AdvSceneSwitcher.action.timer.type.pause"},
	{MacroActionTimer::Action::CONTINUE,
	 "AdvSceneSwitcher.action.timer.type.continue"},
	{MacroActionTimer::Action::RESET,
	 "AdvSceneSwitcher.action.timer.type.reset"},
	{MacroActionTimer::Action::SET_TIME_REMAINING,
	 "AdvSceneSwitcher.action.timer.type.setTimeRemaining"},
};

bool MacroActionTimer::PerformAction()
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return true;
	}

	const auto updateCondition = [this](MacroCondition *condition) {
		if (condition->GetId() != id) {
			return;
		}

		auto timerCondition =
			dynamic_cast<MacroConditionTimer *>(condition);
		if (!timerCondition) {
			return;
		}

		switch (_actionType) {
		case Action::PAUSE:
			timerCondition->Pause();
			break;
		case Action::CONTINUE:
			timerCondition->Continue();
			break;
		case Action::RESET:
			timerCondition->Reset();
			break;
		case Action::SET_TIME_REMAINING:
			timerCondition->_duration.SetTimeRemaining(
				_duration.Seconds());
			break;
		default:
			break;
		}
	};

	const auto updateMacro = [&](Macro *macro) {
		auto conditions = *GetMacroConditions(macro);
		for (auto condition : conditions) {
			updateCondition(condition.get());
		}
	};

	if (!IsGroupMacro(macro.get())) {
		updateMacro(macro.get());
		return true;
	}

	auto macros = GetGroupMacroEntries(macro.get());
	for (const auto &macro : macros) {
		updateMacro(macro.get());
	}
	return true;
}

void MacroActionTimer::LogAction() const
{
	auto macro = _macro.GetMacro();
	if (!macro) {
		return;
	}
	switch (_actionType) {
	case Action::PAUSE:
		ablog(LOG_INFO, "paused timers on \"%s\"",
		      GetMacroName(macro.get()).c_str());
		break;
	case Action::CONTINUE:
		ablog(LOG_INFO, "continued timers on \"%s\"",
		      GetMacroName(macro.get()).c_str());
		break;
	case Action::RESET:
		ablog(LOG_INFO, "reset timers on \"%s\"",
		      GetMacroName(macro.get()).c_str());
		break;
	case Action::SET_TIME_REMAINING:
		ablog(LOG_INFO,
		      "set time remaining of timers on \"%s\" to \"%s\"",
		      GetMacroName(macro.get()).c_str(),
		      _duration.ToString().c_str());
		break;
	default:
		break;
	}
}

bool MacroActionTimer::Save(obs_data_t *obj) const
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
	_actionType = static_cast<Action>(obs_data_get_int(obj, "actionType"));
	return true;
}

std::string MacroActionTimer::GetShortDesc() const
{
	return _macro.Name();
}

std::shared_ptr<MacroAction> MacroActionTimer::Create(Macro *m)
{
	return std::make_shared<MacroActionTimer>(m);
}

std::shared_ptr<MacroAction> MacroActionTimer::Copy() const
{
	return std::make_shared<MacroActionTimer>(*this);
}

void MacroActionTimer::ResolveVariablesToFixedValues()
{
	_duration.ResolveVariables();
}

static void populateTypeSelection(QComboBox *list)
{
	for (const auto &[_, name] : timerActions) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionTimerEdit::MacroActionTimerEdit(
	QWidget *parent, std::shared_ptr<MacroActionTimer> entryData)
	: QWidget(parent),
	  _macros(new MacroSelection(parent)),
	  _duration(new DurationSelection(this)),
	  _timerAction(new QComboBox(this))
{
	populateTypeSelection(_timerAction);

	QWidget::connect(_macros, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(MacroChanged(const QString &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(_timerAction, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionTypeChanged(int)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.timer.entry"),
		     layout,
		     {{"{{macros}}", _macros},
		      {"{{duration}}", _duration},
		      {"{{timerAction}}", _timerAction}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTimerEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_macros->SetCurrentMacro(_entryData->_macro);
	_duration->SetDuration(_entryData->_duration);
	_timerAction->setCurrentIndex(
		static_cast<int>(_entryData->_actionType));
	SetWidgetVisibility();
}

void MacroActionTimerEdit::ActionTypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_actionType = static_cast<MacroActionTimer::Action>(value);
	SetWidgetVisibility();
}

void MacroActionTimerEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_duration->setVisible(_entryData->_actionType ==
			      MacroActionTimer::Action::SET_TIME_REMAINING);
	adjustSize();
}

void MacroActionTimerEdit::DurationChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = dur;
}

void MacroActionTimerEdit::MacroChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_macro = text;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
