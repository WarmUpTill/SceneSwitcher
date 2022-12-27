#include "macro-action-studio-mode.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

const std::string MacroActionSudioMode::id = "studio_mode";

bool MacroActionSudioMode::_registered = MacroActionFactory::Register(
	MacroActionSudioMode::id,
	{MacroActionSudioMode::Create, MacroActionSudioModeEdit::Create,
	 "AdvSceneSwitcher.action.studioMode"});

// TODO: Remove in future version (backwards compatibility)
static const std::string idOld1 = "scene_swap";
static const std::string idOld2 = "preview_scene";
static bool oldRegisterd1 = MacroActionFactory::Register(
	idOld1, {MacroActionSudioMode::Create, MacroActionSudioModeEdit::Create,
		 "AdvSceneSwitcher.action.studioMode"});
static bool oldRegisterd2 = MacroActionFactory::Register(
	idOld2, {MacroActionSudioMode::Create, MacroActionSudioModeEdit::Create,
		 "AdvSceneSwitcher.action.studioMode"});

const static std::map<StudioModeAction, std::string> actionTypes = {
	{StudioModeAction::SWAP_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.swap"},
	{StudioModeAction::SET_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.setScene"},
	{StudioModeAction::ENABLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.enable"},
	{StudioModeAction::DISABLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.disable"},
};

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
	switch (_action) {
	case StudioModeAction::SWAP_SCENE:
		obs_frontend_preview_program_trigger_transition();
		break;
	case StudioModeAction::SET_SCENE: {
		auto s = obs_weak_source_get_source(_scene.GetScene());
		obs_frontend_set_current_preview_scene(s);
		obs_source_release(s);
		break;
	}
	case StudioModeAction::ENABLE_STUDIO_MODE:
		enableStudioMode(true);
		break;
	case StudioModeAction::DISABLE_STUDIO_MODE:
		enableStudioMode(false);
		break;
	default:
		break;
	}

	return true;
}

void MacroActionSudioMode::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO, "performed action \"%s\" with scene \"%s\"",
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
	return true;
}

bool MacroActionSudioMode::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action =
		static_cast<StudioModeAction>(obs_data_get_int(obj, "action"));
	_scene.Load(obj);
	// TODO: Remove in future version (backwards compatibility)
	if (obs_data_get_string(obj, "id") == idOld2) {
		_action = StudioModeAction::SET_SCENE;
	}
	return true;
}

std::string MacroActionSudioMode::GetShortDesc() const
{
	if (_action == StudioModeAction::SET_SCENE) {
		return _scene.ToString();
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSudioModeEdit::MacroActionSudioModeEdit(
	QWidget *parent, std::shared_ptr<MacroActionSudioMode> entryData)
	: QWidget(parent),
	  _actions(new QComboBox()),
	  _scenes(new SceneSelectionWidget(window(), true, true, true, true))
{
	populateActionSelection(_actions);
	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{scenes}}", _scenes},
	};
	QHBoxLayout *mainLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.studioMode.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSudioModeEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_scenes->SetScene(_entryData->_scene);
	_scenes->setVisible(_entryData->_action == StudioModeAction::SET_SCENE);
}

void MacroActionSudioModeEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSudioModeEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<StudioModeAction>(value);
	_scenes->setVisible(_entryData->_action == StudioModeAction::SET_SCENE);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}
