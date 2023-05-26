#include "macro-action-streaming.hpp"
#include "utility.hpp"

#include <util/config-file.h>

namespace advss {

const std::string MacroActionStream::id = "streaming";

bool MacroActionStream::_registered = MacroActionFactory::Register(
	MacroActionStream::id,
	{MacroActionStream::Create, MacroActionStreamEdit::Create,
	 "AdvSceneSwitcher.action.streaming"});

const static std::map<MacroActionStream::Action, std::string> actionTypes = {
	{MacroActionStream::Action::STOP,
	 "AdvSceneSwitcher.action.streaming.type.stop"},
	{MacroActionStream::Action::START,
	 "AdvSceneSwitcher.action.streaming.type.start"},
	{MacroActionStream::Action::KEYFRAME_INTERVAL,
	 "AdvSceneSwitcher.action.streaming.type.keyFrameInterval"},
	{MacroActionStream::Action::SERVER,
	 "AdvSceneSwitcher.action.streaming.type.server"},
	{MacroActionStream::Action::STREAM_KEY,
	 "AdvSceneSwitcher.action.streaming.type.streamKey"},
	{MacroActionStream::Action::USERNAME,
	 "AdvSceneSwitcher.action.streaming.type.username"},
	{MacroActionStream::Action::PASSWORD,
	 "AdvSceneSwitcher.action.streaming.type.password"},
};

constexpr int streamStartCooldown = 5;
std::chrono::high_resolution_clock::time_point MacroActionStream::s_lastAttempt =
	std::chrono::high_resolution_clock::now();

void MacroActionStream::SetKeyFrameInterval() const
{
	const auto configPath = GetPathInProfileDir("streamEncoder.json");
	auto settings =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	if (!settings) {
		blog(LOG_WARNING, "failed to set key frame interval");
		return;
	}
	obs_data_set_int(settings, "keyint_sec", _keyFrameInterval);
	obs_data_save_json_safe(settings, configPath.c_str(), "tmp", "bak");
	obs_data_release(settings);
}

void MacroActionStream::SetStreamSettingsValue(const char *name,
					       const std::string &value,
					       bool enableAuth) const
{
	const auto configPath = GetPathInProfileDir("service.json");
	auto data =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	if (!data) {
		blog(LOG_WARNING, "failed to set %s", name);
		return;
	}
	auto settings = obs_data_get_obj(data, "settings");
	if (!settings) {
		blog(LOG_WARNING, "failed to set %s", name);
		obs_data_release(data);
		return;
	}
	obs_data_set_string(settings, name, value.c_str());
	if (enableAuth) {
		obs_data_set_bool(settings, "use_auth", true);
	}
	obs_data_set_obj(data, "settings", settings);
	auto service = obs_frontend_get_streaming_service();
	obs_service_update(service, settings);
	obs_frontend_save_streaming_service();
	obs_frontend_set_streaming_service(service);
	obs_data_release(settings);
	obs_data_release(data);
}

bool MacroActionStream::CooldownDurationReached() const
{
	auto timePassed = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::high_resolution_clock::now() - s_lastAttempt);
	return timePassed.count() >= streamStartCooldown;
}

bool MacroActionStream::PerformAction()
{
	switch (_action) {
	case Action::STOP:
		if (obs_frontend_streaming_active()) {
			obs_frontend_streaming_stop();
		}
		break;
	case Action::START:
		if (!obs_frontend_streaming_active() &&
		    CooldownDurationReached()) {
			obs_frontend_streaming_start();
			s_lastAttempt =
				std::chrono::high_resolution_clock::now();
		}
		break;
	case Action::KEYFRAME_INTERVAL:
		SetKeyFrameInterval();
		break;
	case Action::SERVER:
		SetStreamSettingsValue("server", _stringValue);
		break;
	case Action::STREAM_KEY:
		SetStreamSettingsValue("key", _stringValue);
		break;
	case Action::USERNAME:
		SetStreamSettingsValue("username", _stringValue, true);
		break;
	case Action::PASSWORD:
		SetStreamSettingsValue("password", _stringValue, true);
		break;
	default:
		break;
	}
	return true;
}

void MacroActionStream::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown streaming action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionStream::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_keyFrameInterval.Save(obj, "keyFrameInterval");
	_stringValue.Save(obj, "stringValue");
	return true;
}

bool MacroActionStream::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_keyFrameInterval.Load(obj, "keyFrameInterval");
	_stringValue.Load(obj, "stringValue");
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionStreamEdit::MacroActionStreamEdit(
	QWidget *parent, std::shared_ptr<MacroActionStream> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _keyFrameInterval(new VariableSpinBox()),
	  _stringValue(new VariableLineEdit(this)),
	  _showPassword(new QPushButton())
{
	_keyFrameInterval->setMinimum(0);
	_keyFrameInterval->setMaximum(25);

	_showPassword->setMaximumWidth(22);
	_showPassword->setFlat(true);
	_showPassword->setStyleSheet(
		"QPushButton { background-color: transparent; border: 0px }");

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(
		_keyFrameInterval,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(KeyFrameIntervalChanged(const NumberVariable<int> &)));
	QWidget::connect(_stringValue, SIGNAL(editingFinished()), this,
			 SLOT(StringValueChanged()));
	QWidget::connect(_showPassword, SIGNAL(pressed()), this,
			 SLOT(ShowPassword()));
	QWidget::connect(_showPassword, SIGNAL(released()), this,
			 SLOT(HidePassword()));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.streaming.entry"),
		     mainLayout,
		     {{"{{actions}}", _actions},
		      {"{{keyFrameInterval}}", _keyFrameInterval},
		      {"{{stringValue}}", _stringValue},
		      {"{{showPassword}}", _showPassword}},
		     false);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionStreamEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_keyFrameInterval->SetValue(_entryData->_keyFrameInterval);
	_stringValue->setText(_entryData->_stringValue);
	SetWidgetVisiblity();
}

void MacroActionStreamEdit::KeyFrameIntervalChanged(
	const NumberVariable<int> &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_keyFrameInterval = value;
}

void MacroActionStreamEdit::StringValueChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_stringValue = _stringValue->text().toStdString();
	SetWidgetVisiblity();
}

void MacroActionStreamEdit::ShowPassword()
{
	SetButtonIcon(_showPassword, ":res/images/visible.svg");
	_stringValue->setEchoMode(QLineEdit::Normal);
}

void MacroActionStreamEdit::HidePassword()
{
	SetButtonIcon(_showPassword, ":res/images/invisible.svg");
	_stringValue->setEchoMode(QLineEdit::PasswordEchoOnEdit);
}

void MacroActionStreamEdit::SetWidgetVisiblity()
{
	if (!_entryData) {
		return;
	}

	const auto action = _entryData->_action;
	_keyFrameInterval->setVisible(
		action == MacroActionStream::Action::KEYFRAME_INTERVAL);
	_stringValue->setVisible(
		action == MacroActionStream::Action::SERVER ||
		action == MacroActionStream::Action::STREAM_KEY ||
		action == MacroActionStream::Action::USERNAME ||
		action == MacroActionStream::Action::PASSWORD);
	if (action == MacroActionStream::Action::PASSWORD ||
	    action == MacroActionStream::Action::STREAM_KEY) {
		_stringValue->setEchoMode(QLineEdit::PasswordEchoOnEdit);
		_showPassword->show();
		HidePassword();
	} else {
		_stringValue->setEchoMode(QLineEdit::Normal);
		_showPassword->hide();
	}
}

void MacroActionStreamEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		auto lock = LockContext();
		_entryData->_action =
			static_cast<MacroActionStream::Action>(value);
	}
	_stringValue->setText(QString(""));
	SetWidgetVisiblity();
}

} // namespace advss
