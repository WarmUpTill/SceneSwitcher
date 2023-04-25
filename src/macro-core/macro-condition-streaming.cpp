#include "macro-condition-streaming.hpp"
#include "switcher-data.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionStream::id = "streaming";

bool MacroConditionStream::_registered = MacroConditionFactory::Register(
	MacroConditionStream::id,
	{MacroConditionStream::Create, MacroConditionStreamEdit::Create,
	 "AdvSceneSwitcher.condition.stream"});

static std::map<MacroConditionStream::Condition, std::string> streamStates = {
	{MacroConditionStream::Condition::STOP,
	 "AdvSceneSwitcher.condition.stream.state.stop"},
	{MacroConditionStream::Condition::START,
	 "AdvSceneSwitcher.condition.stream.state.start"},
	{MacroConditionStream::Condition::STARTING,
	 "AdvSceneSwitcher.condition.stream.state.starting"},
	{MacroConditionStream::Condition::STOPPING,
	 "AdvSceneSwitcher.condition.stream.state.stopping"},
	{MacroConditionStream::Condition::KEYFRAME_INTERVAL,
	 "AdvSceneSwitcher.condition.stream.state.keyFrameInterval"},
};

int MacroConditionStream::GetKeyFrameInterval()
{
	const auto configPath = GetPathInProfileDir("streamEncoder.json");
	obs_data_t *settings =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	if (!settings) {
		return -1;
	}
	int ret = obs_data_get_int(settings, "keyint_sec");
	obs_data_release(settings);
	return ret;
}

bool MacroConditionStream::CheckCondition()
{
	bool match = false;

	bool streamStarting = switcher->lastStreamStartingTime !=
			      _lastStreamStartingTime;
	bool streamStopping = switcher->lastStreamStoppingTime !=
			      _lastStreamStoppingTime;

	switch (_condition) {
	case Condition::STOP:
		match = !obs_frontend_streaming_active();
		break;
	case Condition::START:
		match = obs_frontend_streaming_active();
		break;
	case Condition::STARTING:
		match = streamStarting;
		break;
	case Condition::STOPPING:
		match = streamStopping;
		break;
	case Condition::KEYFRAME_INTERVAL:
		match = GetKeyFrameInterval() == _keyFrameInterval;
		break;
	default:
		break;
	}

	if (streamStarting) {
		_lastStreamStartingTime = switcher->lastStreamStartingTime;
	}
	if (streamStopping) {
		_lastStreamStoppingTime = switcher->lastStreamStoppingTime;
	}
	return match;
}

bool MacroConditionStream::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_condition));
	_keyFrameInterval.Save(obj, "keyFrameInterval");
	return true;
}

bool MacroConditionStream::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "state"));
	_keyFrameInterval.Load(obj, "keyFrameInterval");
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
	: QWidget(parent),
	  _streamState(new QComboBox()),
	  _keyFrameInterval(new VariableSpinBox())
{
	_keyFrameInterval->setMinimum(0);
	_keyFrameInterval->setMaximum(25);

	populateStateSelection(_streamState);

	QWidget::connect(_streamState, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(
		_keyFrameInterval,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(KeyFrameIntervalChanged(const NumberVariable<int> &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.stream.entry"),
		     mainLayout,
		     {{"{{streamState}}", _streamState},
		      {"{{keyFrameInterval}}", _keyFrameInterval}});
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

	auto lock = LockContext();
	_entryData->_condition =
		static_cast<MacroConditionStream::Condition>(value);
	SetWidgetVisiblity();
}

void MacroConditionStreamEdit::KeyFrameIntervalChanged(
	const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_keyFrameInterval = value;
}

void MacroConditionStreamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_streamState->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_keyFrameInterval->SetValue(_entryData->_keyFrameInterval);
	SetWidgetVisiblity();
}

void MacroConditionStreamEdit::SetWidgetVisiblity()
{
	if (!_entryData) {
		return;
	}
	_keyFrameInterval->setVisible(
		_entryData->_condition ==
		MacroConditionStream::Condition::KEYFRAME_INTERVAL);
}

} // namespace advss
