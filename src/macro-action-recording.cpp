#include "headers/macro-action-recording.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionRecord::id = "recording";

bool MacroActionRecord::_registered = MacroActionFactory::Register(
	MacroActionRecord::id,
	{MacroActionRecord::Create, MacroActionRecordEdit::Create,
	 "AdvSceneSwitcher.action.recording"});

const static std::map<RecordAction, std::string> actionTypes = {
	{RecordAction::STOP, "AdvSceneSwitcher.action.recording.type.stop"},
	{RecordAction::START, "AdvSceneSwitcher.action.recording.type.start"},
	{RecordAction::PAUSE, "AdvSceneSwitcher.action.recording.type.pause"},
	{RecordAction::UNPAUSE,
	 "AdvSceneSwitcher.action.recording.type.unpause"},
};

bool MacroActionRecord::PerformAction()
{
	switch (_action) {
	case RecordAction::STOP:
		if (obs_frontend_recording_active()) {
			obs_frontend_recording_stop();
		}
		break;
	case RecordAction::START:
		if (!obs_frontend_recording_active()) {
			obs_frontend_recording_start();
		}
		break;
	case RecordAction::PAUSE:
		if (obs_frontend_recording_active() &&
		    !obs_frontend_recording_paused()) {
			obs_frontend_recording_pause(true);
		}
		break;
	case RecordAction::UNPAUSE:
		if (obs_frontend_recording_active() &&
		    obs_frontend_recording_paused()) {
			obs_frontend_recording_pause(false);
		}
		break;
	default:
		break;
	}
	return true;
}

void MacroActionRecord::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown recording action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionRecord::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionRecord::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<RecordAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionRecordEdit::MacroActionRecordEdit(
	QWidget *parent, std::shared_ptr<MacroActionRecord> entryData)
	: QWidget(parent)
{
	_actions = new QComboBox();
	_pauseHint = new QLabel(obs_module_text(
		"AdvSceneSwitcher.action.recording.pause.hint"));

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{pauseHint}}", _pauseHint},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.recording.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

bool isPauseAction(RecordAction a)
{
	return a == RecordAction::PAUSE || a == RecordAction::UNPAUSE;
}

void MacroActionRecordEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_pauseHint->setVisible(isPauseAction(_entryData->_action));
}

void MacroActionRecordEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<RecordAction>(value);
	_pauseHint->setVisible(isPauseAction(_entryData->_action));
}
