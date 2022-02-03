#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-plugin-state.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionPluginState::id = "plugin_state";

bool MacroConditionPluginState::_registered = MacroConditionFactory::Register(
	MacroConditionPluginState::id,
	{MacroConditionPluginState::Create,
	 MacroConditionPluginStateEdit::Create,
	 "AdvSceneSwitcher.condition.pluginState"});

static std::map<PluginStateCondition, std::string> pluginStateConditionTypes = {
	{PluginStateCondition::SCENE_SWITCHED,
	 "AdvSceneSwitcher.condition.pluginState.state.sceneSwitched"},
	{PluginStateCondition::RUNNING,
	 "AdvSceneSwitcher.condition.pluginState.state.running"},
	{PluginStateCondition::SHUTDOWN,
	 "AdvSceneSwitcher.condition.pluginState.state.shutdown"},
};

MacroConditionPluginState::~MacroConditionPluginState()
{
	if (_condition == PluginStateCondition::SHUTDOWN) {
		switcher->shutdownConditionCount--;
	}
}

bool MacroConditionPluginState::CheckCondition()
{
	switch (_condition) {
	case PluginStateCondition::SCENE_SWITCHED:
		return switcher->macroSceneSwitched;
	case PluginStateCondition::RUNNING:
		return true;
	case PluginStateCondition::SHUTDOWN:
		return switcher->obsIsShuttingDown;
	default:
		break;
	}
	return false;
}

bool MacroConditionPluginState::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	return true;
}

bool MacroConditionPluginState::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_condition = static_cast<PluginStateCondition>(
		obs_data_get_int(obj, "condition"));
	if (_condition == PluginStateCondition::SHUTDOWN) {
		switcher->shutdownConditionCount++;
	}
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : pluginStateConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionPluginStateEdit::MacroConditionPluginStateEdit(
	QWidget *parent, std::shared_ptr<MacroConditionPluginState> entryData)
	: QWidget(parent)
{
	_condition = new QComboBox();

	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	populateConditionSelection(_condition);

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{condition}}", _condition},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.pluginState.entry"),
		switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);

	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionPluginStateEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (_entryData->_condition == PluginStateCondition::SHUTDOWN) {
		switcher->shutdownConditionCount--;
	}
	_entryData->_condition = static_cast<PluginStateCondition>(cond);
	if (_entryData->_condition == PluginStateCondition::SHUTDOWN) {
		switcher->shutdownConditionCount++;
	}
}

void MacroConditionPluginStateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
}
