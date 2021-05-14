#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-recording.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionRecord::id = 8;

bool MacroConditionRecord::_registered = MacroConditionFactory::Register(
	MacroConditionRecord::id,
	{MacroConditionRecord::Create, MacroConditionRecordEdit::Create,
	 "AdvSceneSwitcher.condition.record"});

static std::unordered_map<RecordState, std::string> recordStates = {
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

	if (!stateMatch) {
		_duration.Reset();
		return false;
	}

	return _duration.DurationReached();
}

bool MacroConditionRecord::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_recordState));
	_duration.Save(obj);
	return true;
}

bool MacroConditionRecord::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_recordState = static_cast<RecordState>(obs_data_get_int(obj, "state"));
	_duration.Load(obj);
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
	_duration = new DurationSelection();

	QWidget::connect(_recordState, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));

	populateStateSelection(_recordState);

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{recordState}}", _recordState},
		{"{{duration}}", _duration},
	};

	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.record.entry"),
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

void MacroConditionRecordEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionRecordEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_recordState->setCurrentIndex(
		static_cast<int>(_entryData->_recordState));
	_duration->SetDuration(_entryData->_duration);
}
