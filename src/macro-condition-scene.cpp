#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	0, {MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	    "AdvSceneSwitcher.condition.scene"});

bool MacroConditionScene::CheckCondition()
{
	obs_source_t *rawScene = obs_frontend_get_current_scene();
	OBSWeakSource currentScene = obs_source_get_weak_source(rawScene);
	bool ret = currentScene == _scene;
	obs_weak_source_release(currentScene);
	obs_source_release(rawScene);
	return ret;
}

bool MacroConditionScene::Save()
{
	return false;
}

bool MacroConditionScene::Load()
{
	return false;
}

MacroConditionSceneEdit::MacroConditionSceneEdit(MacroConditionScene *entryData)
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
		obs_module_text("AdvSceneSwitcher.macro.condition.scene.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = GetWeakSourceByQString(text);
}

void MacroConditionSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sceneSelection->setCurrentText(
		GetWeakSourceName(_entryData->_scene).c_str());
}
