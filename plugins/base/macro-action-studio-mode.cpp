#include "macro-action-studio-mode.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

namespace advss {

const std::string MacroActionSudioMode::id = "studio_mode";

bool MacroActionSudioMode::_registered = MacroActionFactory::Register(
	MacroActionSudioMode::id,
	{MacroActionSudioMode::Create, MacroActionStudioModeEdit::Create,
	 "AdvSceneSwitcher.action.studioMode"});

const static std::map<MacroActionSudioMode::Action, std::string> actionTypes = {
	{MacroActionSudioMode::Action::TRANSITION,
	 "AdvSceneSwitcher.action.studioMode.type.transition"},
	{MacroActionSudioMode::Action::TRANSITION_WITH_SWAP,
	 "AdvSceneSwitcher.action.studioMode.type.transitionWithSwap"},
	{MacroActionSudioMode::Action::ENALBE_TRANSITION_SWAP,
	 "AdvSceneSwitcher.action.studioMode.type.transitionSwap.enable"},
	{MacroActionSudioMode::Action::DISABLE_TRANSITION_SWAP,
	 "AdvSceneSwitcher.action.studioMode.type.transitionSwap.disable"},
	{MacroActionSudioMode::Action::TOGGLE_TRANSITION_SWAP,
	 "AdvSceneSwitcher.action.studioMode.type.transitionSwap.toggle"},
	{MacroActionSudioMode::Action::ENALBE_DUPLICATE_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateScene.enable"},
	{MacroActionSudioMode::Action::DISABLE_DUPLICATE_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateScene.disable"},
	{MacroActionSudioMode::Action::TOGGLE_DUPLICATE_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateScene.toggle"},
	{MacroActionSudioMode::Action::ENALBE_DUPLICATE_SOURCE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateSource.enable"},
	{MacroActionSudioMode::Action::DISABLE_DUPLICATE_SOURCE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateSource.disable"},
	{MacroActionSudioMode::Action::TOGGLE_DUPLICATE_SOURCE,
	 "AdvSceneSwitcher.action.studioMode.type.duplicateSource.toggle"},
	{MacroActionSudioMode::Action::ENABLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.enable"},
	{MacroActionSudioMode::Action::DISABLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.disable"},
	{MacroActionSudioMode::Action::TOGGLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.toggle"},
	{MacroActionSudioMode::Action::SET_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.setScene"},
};

template<typename Func, typename... Args>
static void setConfigValueHelper(Func func, Args... args)
{
	auto config = obs_frontend_get_user_config();
	func(config, args...);
	if (config_save(config) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, "failed to save user config!");
	}
}

template<typename Func, typename... Args>
static auto getConfigValueHelper(Func func, Func defaultFunc, Args... args)
	-> std::optional<std::invoke_result_t<Func, config_t *, Args...>>
{
	using Ret = std::invoke_result_t<Func, config_t *, Args...>;

	auto config = obs_frontend_get_user_config();
	if (config_has_user_value(config, args...)) {
		return func(config, args...);
	}
	if (config_has_default_value(config, args...)) {
		return defaultFunc(config, args...);
	}
	return std::optional<Ret>{};
}

// Calling obs_frontend_set_preview_program_mode() directly from a thread that
// is not the main OBS UI thread will lead to undefined behaviour, so we have
// to use this helper function instead - copied from obs-websocket plugin
static void enableStudioMode(bool enable)
{
	if (obs_frontend_preview_program_mode_active() != enable) {
		obs_queue_task(
			OBS_TASK_UI,
			[](void *param) {
				auto studioModeEnabled = (bool *)param;
				obs_frontend_set_preview_program_mode(
					*studioModeEnabled);
			},
			&enable, true);
	}
}

bool MacroActionSudioMode::PerformAction()
{
	obs_frontend_get_user_config();
	switch (_action) {
	case Action::TRANSITION:
		obs_frontend_preview_program_trigger_transition();
		break;
	case Action::TRANSITION_WITH_SWAP: {
		auto swapEnabled = getConfigValueHelper(config_get_bool,
							config_get_default_bool,
							"BasicWindow",
							"SwapScenesMode");

		if (swapEnabled.has_value() && *swapEnabled == true) {
			obs_frontend_preview_program_trigger_transition();
			break;
		}

		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SwapScenesMode", true);
		obs_frontend_preview_program_trigger_transition();
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SwapScenesMode", false);
		break;
	}
	case Action::ENALBE_TRANSITION_SWAP:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SwapScenesMode", true);
		break;
	case Action::DISABLE_TRANSITION_SWAP:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SwapScenesMode", false);
		break;
	case Action::TOGGLE_TRANSITION_SWAP: {
		const auto enabled = getConfigValueHelper(
			config_get_bool, config_get_default_bool, "BasicWindow",
			"SwapScenesMode");
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SwapScenesMode",
				     enabled.has_value() && !*enabled);
		break;
	}
	case Action::ENALBE_DUPLICATE_SCENE:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SceneDuplicationMode", true);
		break;
	case Action::DISABLE_DUPLICATE_SCENE:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SceneDuplicationMode", false);
		break;
	case Action::TOGGLE_DUPLICATE_SCENE: {
		const auto enabled = getConfigValueHelper(
			config_get_bool, config_get_default_bool, "BasicWindow",
			"SceneDuplicationMode");
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "SceneDuplicationMode",
				     enabled.has_value() && !*enabled);
		break;
	}
	case Action::ENALBE_DUPLICATE_SOURCE:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "EditPropertiesMode", true);
		break;
	case Action::DISABLE_DUPLICATE_SOURCE:
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "EditPropertiesMode", false);
		break;
	case Action::TOGGLE_DUPLICATE_SOURCE: {
		const auto enabled = getConfigValueHelper(
			config_get_bool, config_get_default_bool, "BasicWindow",
			"EditPropertiesMode");
		setConfigValueHelper(config_set_bool, "BasicWindow",
				     "EditPropertiesMode",
				     enabled.has_value() && !*enabled);
		break;
	}
	case Action::ENABLE_STUDIO_MODE:
		enableStudioMode(true);
		break;
	case Action::DISABLE_STUDIO_MODE:
		enableStudioMode(false);
		break;
	case Action::TOGGLE_STUDIO_MODE:
		enableStudioMode(!obs_frontend_preview_program_mode_active());
		break;
	case Action::SET_SCENE: {
		auto s = obs_weak_source_get_source(_scene.GetScene());
		obs_frontend_set_current_preview_scene(s);
		obs_source_release(s);
		break;
	}
	default:
		break;
	}

	return true;
}

void MacroActionSudioMode::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		ablog(LOG_INFO, "performed action \"%s\" with scene \"%s\"",
		      it->second.c_str(), _scene.ToString(true).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown studio mode action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSudioMode::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	_scene.Save(obj);
	obs_data_set_int(obj, "version", 1);
	return true;
}

bool MacroActionSudioMode::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_scene.Load(obj);

	if (!obs_data_has_user_value(obj, "version")) {
		enum class OldAction {
			SWAP_SCENE,
			SET_SCENE,
			ENABLE_STUDIO_MODE,
			DISABLE_STUDIO_MODE,
		};

		switch (static_cast<OldAction>(
			obs_data_get_int(obj, "action"))) {
		case OldAction::SWAP_SCENE:
			_action = Action::TRANSITION_WITH_SWAP;
			break;
		case OldAction::SET_SCENE:
			_action = Action::SET_SCENE;
			break;
		case OldAction::ENABLE_STUDIO_MODE:
			_action = Action::ENABLE_STUDIO_MODE;
			break;
		case OldAction::DISABLE_STUDIO_MODE:
			_action = Action::DISABLE_STUDIO_MODE;
			break;
		default:
			break;
		}
	}

	return true;
}

std::string MacroActionSudioMode::GetShortDesc() const
{
	if (_action == Action::SET_SCENE) {
		return _scene.ToString();
	}
	return "";
}

std::shared_ptr<MacroAction> MacroActionSudioMode::Create(Macro *m)
{
	return std::make_shared<MacroActionSudioMode>(m);
}

std::shared_ptr<MacroAction> MacroActionSudioMode::Copy() const
{
	return std::make_shared<MacroActionSudioMode>(*this);
}

void MacroActionSudioMode::ResolveVariablesToFixedValues()
{
	_scene.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[id, name] : actionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(id));
	}
}

MacroActionStudioModeEdit::MacroActionStudioModeEdit(
	QWidget *parent, std::shared_ptr<MacroActionSudioMode> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _scenes(new SceneSelectionWidget(this, true, true, true, true))
{
	populateActionSelection(_actions);
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.studioMode.entry"),
		layout, {{"{{actions}}", _actions}, {"{{scenes}}", _scenes}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionStudioModeEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_scenes->SetScene(_entryData->_scene);
	SetWidgetVisibility();
}

void MacroActionStudioModeEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionStudioModeEdit::SetWidgetVisibility()
{
	_scenes->setVisible(_entryData->_action ==
			    MacroActionSudioMode::Action::SET_SCENE);

	if (_entryData->_action != MacroActionSudioMode::Action::SET_SCENE) {
		_actions->removeItem(_actions->findData(static_cast<int>(
			MacroActionSudioMode::Action::SET_SCENE)));
	}
}

void MacroActionStudioModeEdit::ActionChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionSudioMode::Action>(
		_actions->itemData(index).toInt());
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

} // namespace advss
