#include "macro-action-replay-buffer.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace advss {

const std::string MacroActionReplayBuffer::id = "replay_buffer";

bool MacroActionReplayBuffer::_registered = MacroActionFactory::Register(
	MacroActionReplayBuffer::id,
	{MacroActionReplayBuffer::Create, MacroActionReplayBufferEdit::Create,
	 "AdvSceneSwitcher.action.replay"});

static const std::map<MacroActionReplayBuffer::Action, std::string>
	actionTypes = {
		{MacroActionReplayBuffer::Action::STOP,
		 "AdvSceneSwitcher.action.replay.type.stop"},
		{MacroActionReplayBuffer::Action::START,
		 "AdvSceneSwitcher.action.replay.type.start"},
		{MacroActionReplayBuffer::Action::SAVE,
		 "AdvSceneSwitcher.action.replay.type.save"},
		{MacroActionReplayBuffer::Action::DURATION,
		 "AdvSceneSwitcher.action.replay.type.duration"},
};

bool MacroActionReplayBuffer::PerformAction()
{
	switch (_action) {
	case Action::STOP:
		if (obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_stop();
		}
		break;
	case Action::START:
		if (!obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_start();
		}
		break;
	case Action::SAVE:
		if (obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_save();
		}
		break;
	case Action::DURATION: {
		auto conf = obs_frontend_get_profile_config();
		auto value = std::to_string(_duration.Seconds());
		config_set_string(conf, "SimpleOutput", "RecRBTime",
				  value.c_str());
		config_set_string(conf, "AdvOut", "RecRBTime", value.c_str());

		if (config_save(conf) != CONFIG_SUCCESS) {
			blog(LOG_WARNING,
			     "failed to set replay buffer duration");
		}
		break;
	}
	default:
		break;
	}
	return true;
}

void MacroActionReplayBuffer::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown replay buffer action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionReplayBuffer::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_duration.Save(obj);
	return true;
}

bool MacroActionReplayBuffer::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_duration.Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionReplayBuffer::Create(Macro *m)
{
	return std::make_shared<MacroActionReplayBuffer>(m);
}

std::shared_ptr<MacroAction> MacroActionReplayBuffer::Copy() const
{
	return std::make_shared<MacroActionReplayBuffer>(*this);
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[_, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroActionReplayBufferEdit::MacroActionReplayBufferEdit(
	QWidget *parent, std::shared_ptr<MacroActionReplayBuffer> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _warning(new QLabel(
		  obs_module_text("AdvSceneSwitcher.action.replay.saveWarn"))),
	  _duration(new DurationSelection(this, false, 5.))
{
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.replay.entry"),
		     layout,
		     {{"{{actions}}", _actions}, {"{{duration}}", _duration}});
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(layout);
	mainLayout->addWidget(_warning);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionReplayBufferEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_duration->SetDuration(_entryData->_duration);
	SetWidgetVisibility();
}

void MacroActionReplayBufferEdit::SetWidgetVisibility()
{
	_warning->setVisible(_entryData->_action ==
			     MacroActionReplayBuffer::Action::SAVE);
	_duration->setVisible(_entryData->_action ==
			      MacroActionReplayBuffer::Action::DURATION);
	_warning->setText(obs_module_text(
		(_entryData->_action == MacroActionReplayBuffer::Action::SAVE)
			? "AdvSceneSwitcher.action.replay.saveWarn"
			: "AdvSceneSwitcher.action.replay.durationWarn"));
	_warning->setVisible(_entryData->_action ==
				     MacroActionReplayBuffer::Action::SAVE ||
			     _entryData->_action ==
				     MacroActionReplayBuffer::Action::DURATION);
	adjustSize();
	updateGeometry();
}

void MacroActionReplayBufferEdit::ActionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action =
		static_cast<MacroActionReplayBuffer::Action>(value);

	SetWidgetVisibility();
}

void MacroActionReplayBufferEdit::DurationChanged(const Duration &duration)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = duration;
}

} // namespace advss
