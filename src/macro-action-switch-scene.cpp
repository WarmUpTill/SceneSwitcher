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
	OBSWeakSource transition = nullptr;
	switchScene(_scene, transition, switcher->tansitionOverrideOverride,
		    switcher->adjustActiveTransitionType, switcher->verbose);
	return true;
}

bool MacroActionSwitchScene::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "scene", GetWeakSourceName(_scene).c_str());
	return true;
}

bool MacroActionSwitchScene::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene = GetWeakSourceByName(obs_data_get_string(obj, "scene"));
	return true;
}

MacroActionSwitchSceneEdit::MacroActionSwitchSceneEdit(
	std::shared_ptr<MacroActionSwitchScene> entryData)
{
	_sceneSelection = new QComboBox();

	QWidget::connect(_sceneSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SceneChanged(const QString &)));

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

void MacroActionSwitchSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sceneSelection->setCurrentText(
		GetWeakSourceName(_entryData->_scene).c_str());
}

void MacroActionSwitchSceneEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = GetWeakSourceByQString(text);
}
