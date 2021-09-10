#include "headers/macro-action-scene-swap.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionSceneSwap::id = "scene_swap";

bool MacroActionSceneSwap::_registered = MacroActionFactory::Register(
	MacroActionSceneSwap::id,
	{MacroActionSceneSwap::Create, MacroActionSceneSwapEdit::Create,
	 "AdvSceneSwitcher.action.sceneSwap"});

bool MacroActionSceneSwap::PerformAction()
{
	obs_frontend_preview_program_trigger_transition();
	return true;
}

void MacroActionSceneSwap::LogAction()
{
	vblog(LOG_INFO, "trigger preview to active scene transition");
}

bool MacroActionSceneSwap::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	return true;
}

bool MacroActionSceneSwap::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	return true;
}

MacroActionSceneSwapEdit::MacroActionSceneSwapEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneSwap> entryData)
	: QWidget(parent)
{
	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {};
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.SceneSwap.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
}
