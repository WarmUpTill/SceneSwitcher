#include "headers/macro-action-streaming.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const int MacroActionStream::id = 3;

bool MacroActionStream::_registered = MacroActionFactory::Register(
	MacroActionStream::id,
	{MacroActionStream::Create, MacroActionStreamEdit::Create,
	 "AdvSceneSwitcher.action.streaming"});

const static std::map<StreamAction, std::string> actionTypes = {
	{StreamAction::STOP, "AdvSceneSwitcher.action.streaming.type.stop"},
	{StreamAction::START, "AdvSceneSwitcher.action.streaming.type.start"},
};

bool MacroActionStream::PerformAction()
{
	switch (_action) {
	case StreamAction::STOP:
		if (obs_frontend_streaming_active()) {
			obs_frontend_streaming_stop();
		}
		break;
	case StreamAction::START:
		if (!obs_frontend_streaming_active() &&
		    _retryCooldown.DurationReached()) {
			obs_frontend_streaming_start();
			_retryCooldown.seconds++;
			_retryCooldown.Reset();
		}
		break;
	default:
		break;
	}
	return true;
}

void MacroActionStream::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\"", it->second.c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown streaming action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionStream::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionStream::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<StreamAction>(obs_data_get_int(obj, "action"));
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
	: QWidget(parent)
{
	_actions = new QComboBox();

	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.streaming.entry"),
		     mainLayout, widgetPlaceholders);
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
}

void MacroActionStreamEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<StreamAction>(value);
}
