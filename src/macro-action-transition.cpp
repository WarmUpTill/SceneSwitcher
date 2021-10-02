#include "headers/macro-action-transition.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionTransition::id = "transition";

bool MacroActionTransition::_registered = MacroActionFactory::Register(
	MacroActionTransition::id,
	{MacroActionTransition::Create, MacroActionTransitionEdit::Create,
	 "AdvSceneSwitcher.action.transition"});

bool MacroActionTransition::PerformAction()
{
	if (_setType) {
		auto t =
			obs_weak_source_get_source(_transition.GetTransition());
		obs_frontend_set_current_transition(t);
		obs_source_release(t);
	}
	if (_setDuration) {
		obs_frontend_set_transition_duration(_duration.seconds * 1000);
	}
	return true;
}

void MacroActionTransition::LogAction()
{
	if (_setDuration) {
		vblog(LOG_INFO, "set transition duration to %s",
		      _duration.ToString().c_str());
	}
	if (_setType) {
		vblog(LOG_INFO, "set transition type to \"%s\"",
		      _transition.ToString().c_str());
	}
}

bool MacroActionTransition::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_duration.Save(obj);
	_transition.Save(obj);
	obs_data_set_bool(obj, "setDuration", _setDuration);
	obs_data_set_bool(obj, "setType", _setType);
	return true;
}

bool MacroActionTransition::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration.Load(obj);
	_transition.Load(obj);
	_setDuration = obs_data_get_bool(obj, "setDuration");
	_setType = obs_data_get_bool(obj, "setType");
	return true;
}

std::string MacroActionTransition::GetShortDesc()
{
	return _transition.ToString();
}

MacroActionTransitionEdit::MacroActionTransitionEdit(
	QWidget *parent, std::shared_ptr<MacroActionTransition> entryData)
	: QWidget(parent)
{
	_transitions = new TransitionSelectionWidget(this, false);
	_duration = new DurationSelection(this, false);
	_setType = new QCheckBox();
	_setDuration = new QCheckBox();

	QWidget::connect(_transitions,
			 SIGNAL(TransitionChanged(const TransitionSelection &)),
			 this,
			 SLOT(TransitionChanged(const TransitionSelection &)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_setType, SIGNAL(stateChanged(int)), this,
			 SLOT(SetTypeChanged(int)));
	QWidget::connect(_setDuration, SIGNAL(stateChanged(int)), this,
			 SLOT(SetDurationChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{transitions}}", _transitions},
		{"{{duration}}", _duration},
		{"{{setType}}", _setType},
		{"{{setDuration}}", _setDuration},
	};
	_typeLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.transition.entry.line1"),
		     _typeLayout, widgetPlaceholders);
	_durationLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.transition.entry.line2"),
		     _durationLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(_typeLayout);
	mainLayout->addLayout(_durationLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionTransitionEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_setDuration->setChecked(_entryData->_setDuration);
	_duration->SetDuration(_entryData->_duration);
	_setType->setChecked(_entryData->_setType);
	_transitions->SetTransition(_entryData->_transition);
	_transitions->setEnabled(_entryData->_setType);
	_duration->setEnabled(_entryData->_setDuration);
}

void MacroActionTransitionEdit::TransitionChanged(const TransitionSelection &t)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_transition = t;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionTransitionEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroActionTransitionEdit::SetTypeChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_setType = state;
	_transitions->setEnabled(state);
	if (state) {
		emit HeaderInfoChanged(
			QString::fromStdString(_entryData->GetShortDesc()));
	} else {
		emit HeaderInfoChanged("");
	}
}

void MacroActionTransitionEdit::SetDurationChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_setDuration = state;
	_duration->setEnabled(state);
}
