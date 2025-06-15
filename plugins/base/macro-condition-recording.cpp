#include "macro-condition-recording.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>
#include <thread>

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
		{MacroConditionRecord::Condition::DURATION,
		 "AdvSceneSwitcher.condition.record.state.duration"},
};

static bool SetupRecordingTimer();
static bool recordingTimerIsSetup = SetupRecordingTimer();
static int currentRecordingDurationInSeconds = 0;

bool MacroConditionRecord::CheckCondition()
{
	switch (_condition) {
	case Condition::STOP:
		return !obs_frontend_recording_active();
	case Condition::PAUSE:
		return obs_frontend_recording_paused();
	case Condition::START:
		return obs_frontend_recording_active();
	case Condition::DURATION:
		SetTempVarValue(
			"durationSeconds",
			std::to_string(currentRecordingDurationInSeconds));
		return currentRecordingDurationInSeconds > _duration.Seconds();
	default:
		break;
	}
	return false;
}

bool MacroConditionRecord::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_condition));
	_duration.Save(obj);
	return true;
}

bool MacroConditionRecord::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "state"));
	_duration.Load(obj);
	return true;
}

void MacroConditionRecord::SetCondition(Condition condition)
{
	_condition = condition;
	SetupTempVars();
}

void MacroConditionRecord::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_condition) {
	case Condition::DURATION:
		AddTempvar(
			"durationSeconds",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.recording.durationSeconds"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.recording.durationSeconds.description"));
		break;
	default:
		break;
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
	: QWidget(parent),
	  _condition(new QComboBox(this)),
	  _duration(new DurationSelection(this))
{
	populateStateSelection(_condition);

	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.record.entry"),
		     layout,
		     {{"{{condition}}", _condition},
		      {"{{duration}}", _duration}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionRecordEdit::ConditionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(
		static_cast<MacroConditionRecord::Condition>(value));
	SetWidgetVisibility();
}

void MacroConditionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_condition->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_duration->SetDuration(_entryData->_duration);
	SetWidgetVisibility();
}

void MacroConditionRecordEdit::DurationChanged(const Duration &duration)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = duration;
}

void MacroConditionRecordEdit::SetWidgetVisibility()
{
	_duration->setVisible(_entryData->GetCondition() ==
			      MacroConditionRecord::Condition::DURATION);
}

bool SetupRecordingTimer()
{
	static std::atomic_bool recordingStopped = {true};
	static std::atomic_bool recordingPaused = {false};
	static auto handleRecordingEvents = [](enum obs_frontend_event event,
					       void *) {
		switch (event) {
		case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
			recordingStopped = true;
			break;
		case OBS_FRONTEND_EVENT_RECORDING_STARTED:
			recordingStopped = false;
			break;
		case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
			recordingPaused = true;
			break;
		case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
			recordingPaused = false;
			break;
		default:
			break;
		};
	};
	obs_frontend_add_event_callback(handleRecordingEvents, nullptr);
	std::thread thread([]() {
		while (true) {
			std::this_thread::sleep_for(std::chrono::seconds(1));
			if (recordingStopped) {
				currentRecordingDurationInSeconds = 0;
				continue;
			}
			if (recordingPaused) {
				continue;
			}
			++currentRecordingDurationInSeconds;
		}
	});
	thread.detach();
	return true;
}

} // namespace advss
