#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-streaming.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionStream::id = 7;

bool MacroConditionStream::_registered = MacroConditionFactory::Register(
	MacroConditionStream::id,
	{MacroConditionStream::Create, MacroConditionStreamEdit::Create,
	 "AdvSceneSwitcher.condition.stream"});

static std::map<StreamState, std::string> streamStates = {
	{StreamState::STOP, "AdvSceneSwitcher.condition.stream.state.stop"},
	{StreamState::START, "AdvSceneSwitcher.condition.stream.state.start"},
};

bool MacroConditionStream::CheckCondition()
{
	bool stateMatch = false;
	switch (_streamState) {
	case StreamState::STOP:
		stateMatch = !obs_frontend_streaming_active();
		break;
	case StreamState::START:
		stateMatch = obs_frontend_streaming_active();
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

bool MacroConditionStream::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_streamState));
	_duration.Save(obj);
	return true;
}

bool MacroConditionStream::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_streamState = static_cast<StreamState>(obs_data_get_int(obj, "state"));
	_duration.Load(obj);
	return true;
}

static inline void populateStateSelection(QComboBox *list)
{
	for (auto entry : streamStates) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionStreamEdit::MacroConditionStreamEdit(
	QWidget *parent, std::shared_ptr<MacroConditionStream> entryData)
	: QWidget(parent)
{
	_streamState = new QComboBox();
	_duration = new DurationSelection();

	QWidget::connect(_streamState, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_duration, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(DurationUnitChanged(DurationUnit)));

	populateStateSelection(_streamState);

	QHBoxLayout *mainLayout = new QHBoxLayout;

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{streamState}}", _streamState},
		{"{{duration}}", _duration},
	};

	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.stream.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionStreamEdit::StateChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_streamState = static_cast<StreamState>(value);
}

void MacroConditionStreamEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroConditionStreamEdit::DurationUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.displayUnit = unit;
}

void MacroConditionStreamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_streamState->setCurrentIndex(
		static_cast<int>(_entryData->_streamState));
	_duration->SetDuration(_entryData->_duration);
}
