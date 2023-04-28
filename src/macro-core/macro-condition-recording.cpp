#include "macro-condition-edit.hpp"
#include "macro-condition-recording.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

namespace advss {

const std::string MacroConditionRecord::id = "recording";

bool MacroConditionRecord::_registered = MacroConditionFactory::Register(
	MacroConditionRecord::id,
	{MacroConditionRecord::Create, MacroConditionRecordEdit::Create,
	 "AdvSceneSwitcher.condition.record"});

static std::map<RecordState, std::string> recordStates = {
	{RecordState::STOP, "AdvSceneSwitcher.condition.record.state.stop"},
	{RecordState::PAUSE, "AdvSceneSwitcher.condition.record.state.pause"},
	{RecordState::START, "AdvSceneSwitcher.condition.record.state.start"},
};

bool MacroConditionRecord::CheckCondition()
{
	bool stateMatch = false;
	switch (_recordState) {
	case RecordState::STOP:
		stateMatch = !obs_frontend_recording_active();
		break;
	case RecordState::PAUSE:
		stateMatch = obs_frontend_recording_paused();
		break;
	case RecordState::START:
		stateMatch = obs_frontend_recording_active();
		break;
	default:
		break;
	}
	return stateMatch;
}

bool MacroConditionRecord::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_recordState));
	return true;
}

bool MacroConditionRecord::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_recordState = static_cast<RecordState>(obs_data_get_int(obj, "state"));
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	for (auto entry : recordStates) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionRecordEdit::MacroConditionRecordEdit(
	QWidget *parent, std::shared_ptr<MacroConditionRecord> entryData)
	: QWidget(parent)
{
	_recordState = new QComboBox();

	QWidget::connect(_recordState, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));

	populateStateSelection(_recordState);

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{recordState}}", _recordState},
	};

	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.record.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionRecordEdit::StateChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_recordState = static_cast<RecordState>(value);
}

void MacroConditionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_recordState->setCurrentIndex(
		static_cast<int>(_entryData->_recordState));
}

} // namespace advss
