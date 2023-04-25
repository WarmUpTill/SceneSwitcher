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
};

constexpr int streamStartCooldown = 5;
std::chrono::high_resolution_clock::time_point MacroActionStream::s_lastAttempt =
	std::chrono::high_resolution_clock::now();

void MacroActionStream::SetKeyFrameInterval()
{
	const auto configPath = GetPathInProfileDir("streamEncoder.json");
	obs_data_t *settings =
		obs_data_create_from_json_file_safe(configPath.c_str(), "bak");
	obs_data_set_int(settings, "keyint_sec", _keyFrameInterval);
	if (settings) {
		obs_data_save_json_safe(settings, configPath.c_str(), "tmp",
					"bak");
		obs_data_release(settings);
	}
}

bool MacroActionStream::CooldownDurationReached()
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
	return true;
}

bool MacroActionStream::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_keyFrameInterval.Load(obj, "keyFrameInterval");
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
	  _keyFrameInterval(new VariableSpinBox())
{
	_keyFrameInterval->setMinimum(0);
	_keyFrameInterval->setMaximum(25);

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(
		_keyFrameInterval,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this,
		SLOT(KeyFrameIntervalChanged(const NumberVariable<int> &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.streaming.entry"),
		     mainLayout,
		     {{"{{actions}}", _actions},
		      {"{{keyFrameInterval}}", _keyFrameInterval}});
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

void MacroActionStreamEdit::SetWidgetVisiblity()
{
	if (!_entryData) {
		return;
	}

	_keyFrameInterval->setVisible(
		_entryData->_action ==
		MacroActionStream::Action::KEYFRAME_INTERVAL);
}

void MacroActionStreamEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_action = static_cast<MacroActionStream::Action>(value);
	SetWidgetVisiblity();
}

} // namespace advss
