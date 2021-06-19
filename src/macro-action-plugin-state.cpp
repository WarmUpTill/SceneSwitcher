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
	{PluginStateAction::NO_MATCH_BEHAVIOUR,
	 "AdvSceneSwitcher.action.pluginState.type.noMatch"},
};

const static std::map<NoMatch, std::string> noMatchValues = {
	{NO_SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.dontSwitch"},
	{SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.switchTo"},
	{RANDOM_SWITCH,
	 "AdvSceneSwitcher.generalTab.generalBehavior.onNoMet.switchToRandom"},
};

void stopPlugin()
{
	std::thread t([]() { switcher->Stop(); });
	t.detach();
}

void setNoMatchBehaviour(int value, OBSWeakSource &scene)
{
	switcher->switchIfNotMatching = static_cast<NoMatch>(value);
	if (switcher->switchIfNotMatching == SWITCH) {
		switcher->nonMatchingScene = scene;
	}
}

bool MacroActionPluginState::PerformAction()
{
	switch (_action) {
	case PluginStateAction::STOP:
		stopPlugin();
		break;
	case PluginStateAction::NO_MATCH_BEHAVIOUR:
		setNoMatchBehaviour(_value, _scene);
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
	obs_data_set_int(obj, "value", _value);
	obs_data_set_string(obj, "scene", GetWeakSourceName(_scene).c_str());
	return true;
}

bool MacroActionPluginState::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action =
		static_cast<PluginStateAction>(obs_data_get_int(obj, "action"));
	_value = obs_data_get_int(obj, "value");
	const char *sceneName = obs_data_get_string(obj, "scene");
	_scene = GetWeakSourceByName(sceneName);
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateValueSelection(QComboBox *list,
					  PluginStateAction action)
{
	if (action == PluginStateAction::NO_MATCH_BEHAVIOUR) {
		for (auto entry : noMatchValues) {
			list->addItem(obs_module_text(entry.second.c_str()));
		}
	}
}

MacroActionPluginStateEdit::MacroActionPluginStateEdit(
	QWidget *parent, std::shared_ptr<MacroActionPluginState> entryData)
	: QWidget(parent)
{
	_actions = new QComboBox();
	_values = new QComboBox();
	_scenes = new QComboBox();

	populateActionSelection(_actions);
	populateSceneSelection(_scenes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_values, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ValueChanged(int)));
	QWidget::connect(_scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{values}}", _values},
		{"{{scenes}}", _scenes},
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
	populateValueSelection(_values, _entryData->_action);
	_values->setCurrentIndex(_entryData->_value);
	_scenes->setCurrentText(GetWeakSourceName(_entryData->_scene).c_str());
	SetWidgetVisibility(_entryData->_action, _entryData->_value);
}

void MacroActionPluginStateEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<PluginStateAction>(value);
	SetWidgetVisibility(_entryData->_action, _entryData->_value);
}

void MacroActionPluginStateEdit::ValueChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_value = value;
	SetWidgetVisibility(_entryData->_action, _entryData->_value);
}

void MacroActionPluginStateEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = GetWeakSourceByQString(text);
}

void MacroActionPluginStateEdit::SetWidgetVisibility(PluginStateAction action,
						     int value)
{
	_values->hide();
	_scenes->hide();
	if (action == PluginStateAction::NO_MATCH_BEHAVIOUR) {
		_values->show();
		if ((NoMatch)value == SWITCH) {
			_scenes->show();
		}
	}
}
