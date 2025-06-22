#include "macro-action-studio-mode.hpp"
#include "layout-helpers.hpp"

#include <obs-frontend-api.h>

namespace advss {

const std::string MacroActionSudioMode::id = "studio_mode";

bool MacroActionSudioMode::_registered = MacroActionFactory::Register(
	MacroActionSudioMode::id,
	{MacroActionSudioMode::Create, MacroActionStudioModeEdit::Create,
	 "AdvSceneSwitcher.action.studioMode"});

const static std::map<MacroActionSudioMode::Action, std::string> actionTypes = {
	{MacroActionSudioMode::Action::SWAP_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.swap"},
	{MacroActionSudioMode::Action::SET_SCENE,
	 "AdvSceneSwitcher.action.studioMode.type.setScene"},
	{MacroActionSudioMode::Action::ENABLE_STUDIO_MODE,
	 "AdvSceneSwitcher.action.studioMode.type.enable"},
	{MacroActionSudioMode::Action::DISABLE_STUDIO_MODE,
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
	case Action::SWAP_SCENE:
		obs_frontend_preview_program_trigger_transition();
		break;
	case Action::SET_SCENE: {
		auto s = obs_weak_source_get_source(_scene.GetScene());
		obs_frontend_set_current_preview_scene(s);
		obs_source_release(s);
		break;
	}
	case Action::ENABLE_STUDIO_MODE:
		enableStudioMode(true);
		break;
	case Action::DISABLE_STUDIO_MODE:
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
	return true;
}

bool MacroActionSudioMode::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_scene.Load(obj);
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

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{actions}}", _actions},
		{"{{scenes}}", _scenes},
	};
	QHBoxLayout *mainLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.studioMode.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

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
