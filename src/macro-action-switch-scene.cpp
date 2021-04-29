#include "headers/macro-action-switch-scene.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const int MacroActionSwitchScene::id = 0;

bool MacroActionSwitchScene::_registered = MacroActionFactory::Register(
	MacroActionSwitchScene::id,
	{MacroActionSwitchScene::Create, MacroActionSwitchSceneEdit::Create,
	 "AdvSceneSwitcher.action.switchScene"});

bool MacroActionSwitchScene::PerformAction()
{
	OBSWeakSource scene = getScene();
	switchScene(scene, transition, switcher->tansitionOverrideOverride,
		    switcher->adjustActiveTransitionType, switcher->verbose);
	return true;
}

bool MacroActionSwitchScene::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	SceneSwitcherEntry::save(obj);
	return true;
}

bool MacroActionSwitchScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	SceneSwitcherEntry::load(obj);
	return true;
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	std::shared_ptr<MacroActionSwitchScene> entryData)
	: SwitchWidget(nullptr, entryData.get(), true, true)
{
	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.macro.action.scene.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	SwitchWidget::loading = false;
}
