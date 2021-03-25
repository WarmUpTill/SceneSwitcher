#include "headers/macro-action-switch-scene.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool MacroActionSwitchScene::PerformAction()
{
	auto scene = obs_weak_source_get_source(_scene);
	obs_frontend_set_current_scene(scene);
	obs_source_release(scene);

	return true;
}

bool MacroActionSwitchScene::Save()
{
	return false;
}

bool MacroActionSwitchScene::Load()
{
	return false;
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	MacroActionSwitchScene *entryData)
{
	_sceneSelection = new QComboBox();

	QWidget::connect(_sceneSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SceneChanged(int)));

	AdvSceneSwitcher::populateSceneSelection(_sceneSelection);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _sceneSelection},
	};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.macro.action.scene.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSwitchSceneEdit::UpdateEntryData() {}

void MacroActionSwitchSceneEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = GetWeakSourceByQString(text);
}
