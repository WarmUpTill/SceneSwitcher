#include "headers/macro-action-plugin-state.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <thread>

const std::string MacroActionPluginState::id = "plugin_state";

bool MacroActionPluginState::_registered = MacroActionFactory::Register(
	MacroActionPluginState::id,
	{MacroActionPluginState::Create, MacroActionPluginStateEdit::Create,
	 "AdvSceneSwitcher.action.PluginState"});

const static std::map<PluginStateAction, std::string> actionTypes = {
	{PluginStateAction::STOP,
	 "AdvSceneSwitcher.action.pluginState.type.stop"},
};

void stopPlugin()
{
	std::thread t([]() { switcher->Stop(); });
	t.detach();
}

bool MacroActionPluginState::PerformAction()
{
	switch (_action) {
	case PluginStateAction::STOP:
		stopPlugin();
		break;
	default:
		break;
	}
	return true;
}

void MacroActionPluginState::LogAction()
{
	auto it = actionTypes.find(_action);
	switch (_action) {
	case PluginStateAction::STOP:
		blog(LOG_INFO, "stop() called by macro");
		break;
	default:
		blog(LOG_WARNING, "ignored unknown pluginState action %d",
		     static_cast<int>(_action));
		break;
	}
}

bool MacroActionPluginState::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionPluginState::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action =
		static_cast<PluginStateAction>(obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionPluginStateEdit::MacroActionPluginStateEdit(
	QWidget *parent, std::shared_ptr<MacroActionPluginState> entryData)
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
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.pluginState.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionPluginStateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
}

void MacroActionPluginStateEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<PluginStateAction>(value);
}
