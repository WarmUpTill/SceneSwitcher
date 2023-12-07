#include "macro-condition-recording.hpp"
#include "macro.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionRecord::id = "recording";

bool MacroConditionRecord::_registered = MacroConditionFactory::Register(
	MacroConditionRecord::id,
	{MacroConditionRecord::Create, MacroConditionRecordEdit::Create,
	 "AdvSceneSwitcher.condition.record"});

const static std::map<MacroConditionRecord::Condition, std::string>
	recordStates = {
		{MacroConditionRecord::Condition::STOP,
		 "AdvSceneSwitcher.condition.record.state.stop"},
		{MacroConditionRecord::Condition::PAUSE,
		 "AdvSceneSwitcher.condition.record.state.pause"},
		{MacroConditionRecord::Condition::START,
		 "AdvSceneSwitcher.condition.record.state.start"},
		{MacroConditionRecord::Condition::SAVE_DONE,
		 "AdvSceneSwitcher.condition.record.state.saveDone"},
};

MacroConditionRecord::MacroConditionRecord(Macro *m) : MacroCondition(m)
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (!output) {
		return;
	}
	auto sh = obs_output_get_signal_handler(output);
	signal_handler_connect(sh, "stop", StopSignal, this);
}

MacroConditionRecord::~MacroConditionRecord()
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (!output) {
		return;
	}
	auto sh = obs_output_get_signal_handler(output);
	signal_handler_disconnect(sh, "stop", StopSignal, this);
}

bool MacroConditionRecord::CheckCondition()
{
	switch (_condition) {
	case Condition::STOP:
		return !obs_frontend_recording_active();
	case Condition::PAUSE:
		return obs_frontend_recording_paused();
	case Condition::START:
		return obs_frontend_recording_active();
	case Condition::SAVE_DONE:
		if (_recordingSaveDone) {
			_recordingSaveDone = false;
			return true;
		}
		return false;
	default:
		break;
	}
	return false;
}

bool MacroConditionRecord::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_condition));
	return true;
}

bool MacroConditionRecord::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "state"));
	return true;
}

void MacroConditionRecord::StopSignal(void *c, calldata_t *data)
{
	auto condition = static_cast<MacroConditionRecord *>(c);
	const auto macro = condition->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}

	long long code = 0;
	if (!calldata_get_int(data, "code", &code)) {
		condition->_recordingSaveDone = false;
	}

	if (code == OBS_OUTPUT_SUCCESS) {
		condition->_recordingSaveDone = true;
	}
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

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionRecord::Condition>(value);
}

void MacroConditionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_recordState->setCurrentIndex(static_cast<int>(_entryData->_condition));
}

} // namespace advss
