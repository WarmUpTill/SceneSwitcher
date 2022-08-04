#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-replay-buffer.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionReplayBuffer::id = "replay_buffer";

bool MacroConditionReplayBuffer::_registered = MacroConditionFactory::Register(
	MacroConditionReplayBuffer::id, {MacroConditionReplayBuffer::Create,
					 MacroConditionReplayBufferEdit::Create,
					 "AdvSceneSwitcher.condition.replay"});

static std::map<ReplayBufferState, std::string> ReplayBufferStates = {
	{ReplayBufferState::STOP,
	 "AdvSceneSwitcher.condition.replay.state.stopped"},
	{ReplayBufferState::START,
	 "AdvSceneSwitcher.condition.replay.state.started"},
	{ReplayBufferState::SAVE,
	 "AdvSceneSwitcher.condition.replay.state.saved"},
};

bool MacroConditionReplayBuffer::CheckCondition()
{
	bool stateMatch = false;
	switch (_state) {
	case ReplayBufferState::STOP:
		stateMatch = !obs_frontend_replay_buffer_active();
		break;
	case ReplayBufferState::START:
		stateMatch = obs_frontend_replay_buffer_active();
		break;
	case ReplayBufferState::SAVE:
		if (switcher->replayBufferSaved) {
			stateMatch = true;
			switcher->replayBufferSaved = false;
		}
		break;
	default:
		break;
	}
	return stateMatch;
}

bool MacroConditionReplayBuffer::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	return true;
}

bool MacroConditionReplayBuffer::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_state = static_cast<ReplayBufferState>(obs_data_get_int(obj, "state"));
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	for (auto entry : ReplayBufferStates) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionReplayBufferEdit::MacroConditionReplayBufferEdit(
	QWidget *parent, std::shared_ptr<MacroConditionReplayBuffer> entryData)
	: QWidget(parent)
{
	_state = new QComboBox();

	QWidget::connect(_state, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));

	populateStateSelection(_state);

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{state}}", _state},
	};

	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.replay.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionReplayBufferEdit::StateChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_state = static_cast<ReplayBufferState>(value);
}

void MacroConditionReplayBufferEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_state->setCurrentIndex(static_cast<int>(_entryData->_state));
}
