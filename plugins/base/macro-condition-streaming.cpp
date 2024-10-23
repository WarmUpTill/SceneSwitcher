#include "macro-condition-streaming.hpp"
#include "profile-helpers.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

namespace advss {

const std::string MacroConditionStream::id = "streaming";

bool MacroConditionStream::_registered = MacroConditionFactory::Register(
	MacroConditionStream::id,
	{MacroConditionStream::Create, MacroConditionStreamEdit::Create,
	 "AdvSceneSwitcher.condition.stream"});

const static std::map<MacroConditionStream::Condition, std::string> conditions =
	{
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
		{MacroConditionStream::Condition::STREAM_KEY,
		 "AdvSceneSwitcher.condition.stream.state.streamKey"},
		{MacroConditionStream::Condition::SERVICE,
		 "AdvSceneSwitcher.condition.stream.state.service"},
};

static bool setupStreamingEventHandler();
static bool steamingEventHandlerIsSetup = setupStreamingEventHandler();
static std::chrono::high_resolution_clock::time_point streamStartTime{};
static std::chrono::high_resolution_clock::time_point streamStopTime{};

static bool setupStreamingEventHandler()
{
	static auto handleStreamingEvents = [](enum obs_frontend_event event,
					       void *) {
		switch (event) {
		case OBS_FRONTEND_EVENT_STREAMING_STARTING:
			streamStartTime =
				std::chrono::high_resolution_clock::now();
			break;
		case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
			streamStopTime =
				std::chrono::high_resolution_clock::now();
			break;
		default:
			break;
		};
	};
	obs_frontend_add_event_callback(handleStreamingEvents, nullptr);
	return true;
}

static std::optional<std::string> getStreamKey()
{
	const auto configPath = GetPathInProfileDir("service.json");
	OBSDataAutoRelease data =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	if (!data) {
		return {};
	}
	OBSDataAutoRelease settings = obs_data_get_obj(data, "settings");
	if (!settings) {
		return {};
	}
	return obs_data_get_string(settings, "key");
}

std::optional<int> getKeyFrameInterval()
{
	const auto configPath = GetPathInProfileDir("streamEncoder.json");
	OBSDataAutoRelease settings =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	if (!settings) {
		return {};
	}
	int ret = obs_data_get_int(settings, "keyint_sec");
	return ret;
}

static std::string getCurrentServiceName()
{
	auto service = obs_frontend_get_streaming_service();
	if (!service) {
		return "None";
	}

	auto id = obs_service_get_id(service);
	if (strcmp(id, "rtmp_common") != 0) {
		return obs_service_get_display_name(id);
	}

	const auto configPath = GetPathInProfileDir("service.json");
	OBSDataAutoRelease serviceSettings =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	OBSDataAutoRelease settings =
		obs_data_get_obj(serviceSettings, "settings");
	auto serviceName = obs_data_get_string(settings, "service");

	if (serviceName) {
		return serviceName;
	}
	return "None";
}

bool MacroConditionStream::CheckCondition()
{
	bool match = false;

	bool streamStarting = streamStartTime != _lastStreamStartingTime;
	bool streamStopping = streamStopTime != _lastStreamStoppingTime;
	auto keyFrameInterval = getKeyFrameInterval();
	auto streamKey = getStreamKey();
	auto serviceName = getCurrentServiceName();

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
		if (keyFrameInterval) {
			match = *keyFrameInterval == _keyFrameInterval;
		}
		break;
	case Condition::STREAM_KEY:
		if (streamKey) {
			match = streamKey == std::string(_streamKey);
		}
		break;
	case Condition::SERVICE:
		if (_regex.Enabled()) {
			match = _regex.Matches(serviceName, _serviceName);
		} else {
			match = std::string(_serviceName) == serviceName;
		}
		break;
	default:
		break;
	}

	if (streamStarting) {
		_lastStreamStartingTime = streamStartTime;
	}
	if (streamStopping) {
		_lastStreamStoppingTime = streamStopTime;
	}

	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::high_resolution_clock::now() - streamStartTime);
	const auto streamDurationSeconds =
		obs_frontend_streaming_active() ? seconds.count() : 0;
	SetTempVarValue("durationSeconds",
			std::to_string(streamDurationSeconds));
	SetTempVarValue("serviceName", serviceName);
	if (keyFrameInterval) {
		SetTempVarValue("keyframeInterval",
				std::to_string(*keyFrameInterval));
	}
	if (streamKey) {
		SetTempVarValue("streamKey", *streamKey);
	}

	return match;
}

bool MacroConditionStream::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "state", static_cast<int>(_condition));
	_keyFrameInterval.Save(obj, "keyFrameInterval");
	_streamKey.Save(obj, "streamKey");
	_serviceName.Save(obj, "serviceName");
	_regex.Save(obj);
	return true;
}

bool MacroConditionStream::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "state"));
	_keyFrameInterval.Load(obj, "keyFrameInterval");
	_streamKey.Load(obj, "streamKey");
	_serviceName.Load(obj, "serviceName");
	_regex.Load(obj);
	return true;
}

void MacroConditionStream::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"keyframeInterval",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.keyframeInterval"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.keyframeInterval.description"));
	AddTempvar("streamKey",
		   obs_module_text(
			   "AdvSceneSwitcher.tempVar.streaming.streamKey"));
	AddTempvar(
		"durationSeconds",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.durationSeconds"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.durationSeconds.description"));
	AddTempvar(
		"serviceName",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.serviceName"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.streaming.serviceName.description"));
}

static void populateConditionSelection(QComboBox *list)
{
	for (const auto &[_, name] : conditions) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

MacroConditionStreamEdit::MacroConditionStreamEdit(
	QWidget *parent, std::shared_ptr<MacroConditionStream> entryData)
	: QWidget(parent),
	  _conditions(new QComboBox()),
	  _keyFrameInterval(new VariableSpinBox()),
	  _streamKey(new VariableLineEdit(this)),
	  _serviceName(new VariableLineEdit(this)),
	  _currentService(new AutoUpdateTooltipLabel(
		  this,
		  []() {
			  QString formatString = obs_module_text(
				  "AdvSceneSwitcher.condition.stream.service.tooltip");
			  return formatString.arg(QString::fromStdString(
				  getCurrentServiceName()));
		  })),
	  _regex(new RegexConfigWidget(this))
{
	_keyFrameInterval->setMinimum(0);
	_keyFrameInterval->setMaximum(25);

	QString path = GetThemeTypeName() == "Light"
			       ? ":/res/images/help.svg"
			       : ":/res/images/help_light.svg";
	QIcon icon(path);
	QPixmap pixmap = icon.pixmap(QSize(16, 16));
	_currentService->setPixmap(pixmap);

	_streamKey->setEchoMode(QLineEdit::PasswordEchoOnEdit);

	populateConditionSelection(_conditions);

	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(
		_keyFrameInterval,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(KeyFrameIntervalChanged(const NumberVariable<int> &)));
	QWidget::connect(_serviceName, SIGNAL(editingFinished()), this,
			 SLOT(ServiceNameChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_streamKey, SIGNAL(editingFinished()), this,
			 SLOT(StreamKeyChanged()));

	auto layout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.stream.entry"),
		     layout,
		     {{"{{streamState}}", _conditions},
		      {"{{keyFrameInterval}}", _keyFrameInterval},
		      {"{{streamKey}}", _streamKey},
		      {"{{serviceName}}", _serviceName},
		      {"{{regex}}", _regex},
		      {"{{currentService}}", _currentService}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionStreamEdit::ConditionChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_condition =
		static_cast<MacroConditionStream::Condition>(value);
	SetWidgetVisibility();
}

void MacroConditionStreamEdit::KeyFrameIntervalChanged(
	const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_keyFrameInterval = value;
}

void MacroConditionStreamEdit::ServiceNameChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_serviceName = _serviceName->text().toStdString();
}

void MacroConditionStreamEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = regex;
}

void MacroConditionStreamEdit::StreamKeyChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_streamKey = _streamKey->text().toStdString();
}

void MacroConditionStreamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_streamKey->setText(_entryData->_streamKey);
	_serviceName->setText(_entryData->_serviceName);
	_regex->SetRegexConfig(_entryData->_regex);
	_keyFrameInterval->SetValue(_entryData->_keyFrameInterval);
	SetWidgetVisibility();
}

void MacroConditionStreamEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_keyFrameInterval->setVisible(
		_entryData->_condition ==
		MacroConditionStream::Condition::KEYFRAME_INTERVAL);
	_streamKey->setVisible(_entryData->_condition ==
			       MacroConditionStream::Condition::STREAM_KEY);
	const bool isCheckingStreamingService =
		_entryData->_condition ==
		MacroConditionStream::Condition::SERVICE;
	_serviceName->setVisible(isCheckingStreamingService);
	_regex->setVisible(isCheckingStreamingService);
	_currentService->setVisible(isCheckingStreamingService);
}

} // namespace advss
