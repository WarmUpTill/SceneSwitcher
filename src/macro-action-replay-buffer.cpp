#ifdef REPLAYBUFFER_SUPPORTED

#include "headers/macro-action-replay-buffer.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionReplayBuffer::id = "replay_buffer";

bool MacroActionReplayBuffer::_registered = MacroActionFactory::Register(
	MacroActionReplayBuffer::id,
	{MacroActionReplayBuffer::Create, MacroActionReplayBufferEdit::Create,
	 "AdvSceneSwitcher.action.replay"});

const static std::map<ReplayBufferAction, std::string> actionTypes = {
	{ReplayBufferAction::STOP, "AdvSceneSwitcher.action.replay.type.stop"},
	{ReplayBufferAction::START,
	 "AdvSceneSwitcher.action.replay.type.start"},
	{ReplayBufferAction::SAVE, "AdvSceneSwitcher.action.replay.type.save"},
};

bool MacroActionReplayBuffer::PerformAction()
{
	switch (_action) {
	case ReplayBufferAction::STOP:
		if (obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_stop();
		}
		break;
	case ReplayBufferAction::START:
		if (!obs_frontend_replay_buffer_active()) {
			obs_frontend_replay_buffer_start();
		}
		break;
	case ReplayBufferAction::SAVE:
		if (obs_frontend_replay_buffer_active() &&
		    _duration.DurationReached()) {
			obs_frontend_replay_buffer_save();
			// Default buffer size is 20s so waiting for 10s before
			// trying to save again seems reasonable
			_duration.seconds = 10;
			_duration.Reset();
		}
		break;
	default:
		break;
	}
	return true;
}

void MacroActionReplayBuffer::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown replay buffer action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionReplayBuffer::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionReplayBuffer::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<ReplayBufferAction>(
		obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionReplayBufferEdit::MacroActionReplayBufferEdit(
	QWidget *parent, std::shared_ptr<MacroActionReplayBuffer> entryData)
	: QWidget(parent)
{
	_actions = new QComboBox();

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.replay.entry"),
		     mainLayout, widgetPlaceholders);
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
}

void MacroActionReplayBufferEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<ReplayBufferAction>(value);
}

#endif
