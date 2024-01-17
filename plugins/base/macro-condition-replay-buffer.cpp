#include "macro-condition-replay-buffer.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionReplayBuffer::id = "replay_buffer";

bool MacroConditionReplayBuffer::_registered = MacroConditionFactory::Register(
	MacroConditionReplayBuffer::id, {MacroConditionReplayBuffer::Create,
					 MacroConditionReplayBufferEdit::Create,
					 "AdvSceneSwitcher.condition.replay"});

const static std::map<MacroConditionReplayBuffer::Condition, std::string>
	conditions = {
		{MacroConditionReplayBuffer::Condition::STOP,
		 "AdvSceneSwitcher.condition.replay.state.stopped"},
		{MacroConditionReplayBuffer::Condition::START,
		 "AdvSceneSwitcher.condition.replay.state.started"},
		{MacroConditionReplayBuffer::Condition::SAVE,
		 "AdvSceneSwitcher.condition.replay.state.saved"},
};

static bool replayBufferSaved = false;

static std::chrono::high_resolution_clock::time_point replayBufferSaveTime{};
static bool setupReplayBufferEventHandler();
static bool replayBufferEventHandlerIsSetup = setupReplayBufferEventHandler();

static bool setupReplayBufferEventHandler()
{
	static auto handleReplayBufferEvent = [](enum obs_frontend_event event,
						 void *) {
		switch (event) {
		case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
			replayBufferSaveTime =
				std::chrono::high_resolution_clock::now();
			break;
		default:
			break;
		};
	};
	obs_frontend_add_event_callback(handleReplayBufferEvent, nullptr);
	return true;
}

bool MacroConditionReplayBuffer::ReplayBufferWasSaved()
{
	bool timersAreInitialized =
		_saveTime.time_since_epoch().count() != 0 &&
		replayBufferSaveTime.time_since_epoch().count() != 0;
	bool newSaveOccurred = timersAreInitialized &&
			       _saveTime != replayBufferSaveTime;
	_saveTime = replayBufferSaveTime;
	return newSaveOccurred;
}

bool MacroConditionReplayBuffer::CheckCondition()
{
	switch (_state) {
	case Condition::STOP:
		return !obs_frontend_replay_buffer_active();
	case Condition::START:
		return obs_frontend_replay_buffer_active();
	case Condition::SAVE:
		return ReplayBufferWasSaved();
	default:
		break;
	}
	return false;
}

bool MacroConditionReplayBuffer::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	return true;
}

bool MacroConditionReplayBuffer::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_state = static_cast<Condition>(obs_data_get_int(obj, "state"));
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	for (const auto &entry : conditions) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionReplayBufferEdit::MacroConditionReplayBufferEdit(
	QWidget *parent, std::shared_ptr<MacroConditionReplayBuffer> entryData)
	: QWidget(parent), _state(new QComboBox())
{
	QWidget::connect(_state, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));

	populateStateSelection(_state);

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.replay.entry"),
		     layout, {{"{{state}}", _state}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionReplayBufferEdit::StateChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_state =
		static_cast<MacroConditionReplayBuffer::Condition>(value);
}

void MacroConditionReplayBufferEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_state->setCurrentIndex(static_cast<int>(_entryData->_state));
}

} // namespace advss
