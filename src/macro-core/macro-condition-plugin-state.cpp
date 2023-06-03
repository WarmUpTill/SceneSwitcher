#include "macro-condition-plugin-state.hpp"
#include "switcher-data.hpp"
#include "utility.hpp"

namespace advss {

const uint32_t MacroConditionPluginState::_version = 1;

const std::string MacroConditionPluginState::id = "plugin_state";

bool MacroConditionPluginState::_registered = MacroConditionFactory::Register(
	MacroConditionPluginState::id,
	{MacroConditionPluginState::Create,
	 MacroConditionPluginStateEdit::Create,
	 "AdvSceneSwitcher.condition.pluginState"});

const static std::map<MacroConditionPluginState::Condition, std::string>
	pluginStateConditionTypes = {
		{MacroConditionPluginState::Condition::PLUGIN_START,
		 "AdvSceneSwitcher.condition.pluginState.state.start"},
		{MacroConditionPluginState::Condition::PLUGIN_RESTART,
		 "AdvSceneSwitcher.condition.pluginState.state.restart"},
		{MacroConditionPluginState::Condition::PLUGIN_RUNNING,
		 "AdvSceneSwitcher.condition.pluginState.state.running"},
		{MacroConditionPluginState::Condition::OBS_SHUTDOWN,
		 "AdvSceneSwitcher.condition.pluginState.state.shutdown"},
		{MacroConditionPluginState::Condition::SCENE_COLLECTION_CHANGE,
		 "AdvSceneSwitcher.condition.pluginState.state.sceneCollection"},
		{MacroConditionPluginState::Condition::PLUGIN_SCENE_CHANGE,
		 "AdvSceneSwitcher.condition.pluginState.state.sceneSwitched"},
};

MacroConditionPluginState::~MacroConditionPluginState()
{
	if (_condition == Condition::OBS_SHUTDOWN) {
		switcher->shutdownConditionCount--;
	}
}

bool MacroConditionPluginState::CheckCondition()
{
	switch (_condition) {
	case Condition::PLUGIN_SCENE_CHANGE:
		return switcher->macroSceneSwitched;
	case Condition::PLUGIN_RUNNING:
		return true;
	case Condition::OBS_SHUTDOWN:
		return switcher->obsIsShuttingDown;
	case Condition::PLUGIN_START:
		return switcher->firstInterval;
	case Condition::PLUGIN_RESTART:
		return switcher->firstIntervalAfterStop;
	case Condition::SCENE_COLLECTION_CHANGE:
		if (_firstCheckAfterSceneCollectionChange) {
			_firstCheckAfterSceneCollectionChange = false;
			return true;
		}
		return false;
	}
	return false;
}

bool MacroConditionPluginState::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_int(obj, "version", _version);
	return true;
}

bool MacroConditionPluginState::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);

	// TODO: Remove fallback for older version
	if (!obs_data_has_user_value(obj, "version")) {
		enum class PluginStateCondition {
			SCENE_SWITCHED,
			RUNNING,
			SHUTDOWN,
		};
		auto oldValue = static_cast<PluginStateCondition>(
			obs_data_get_int(obj, "condition"));
		switch (oldValue) {
		case PluginStateCondition::SCENE_SWITCHED:
			_condition = Condition::PLUGIN_SCENE_CHANGE;
			break;
		case PluginStateCondition::RUNNING:
			_condition = Condition::PLUGIN_RUNNING;
			break;
		case PluginStateCondition::SHUTDOWN:
			_condition = Condition::OBS_SHUTDOWN;
			break;
		}
	} else {
		_condition = static_cast<Condition>(
			obs_data_get_int(obj, "condition"));
	}
	if (_condition == Condition::OBS_SHUTDOWN) {
		switcher->shutdownConditionCount++;
	}
	return true;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (const auto &[action, name] : pluginStateConditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
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
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.pluginState.entry"),
		switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(switchLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionPluginStateEdit::ConditionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	if (_entryData->_condition ==
	    MacroConditionPluginState::Condition::OBS_SHUTDOWN) {
		switcher->shutdownConditionCount--;
	}
	_entryData->_condition =
		static_cast<MacroConditionPluginState::Condition>(
			_condition->itemData(idx).toInt());
	if (_entryData->_condition ==
	    MacroConditionPluginState::Condition::OBS_SHUTDOWN) {
		switcher->shutdownConditionCount++;
	}
}

void MacroConditionPluginStateEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_condition->setCurrentIndex(
		_condition->findData(static_cast<int>(_entryData->_condition)));
}

} // namespace advss
