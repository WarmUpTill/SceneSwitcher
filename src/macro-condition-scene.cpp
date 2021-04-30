#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionScene::id = 0;

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	MacroConditionScene::id,
	{MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	 "AdvSceneSwitcher.condition.scene"});

static std::unordered_map<SceneType, std::string> sceneTypes = {
	{SceneType::CURRENT, "AdvSceneSwitcher.action.scene.type.current"},
	{SceneType::PREVIOUS, "AdvSceneSwitcher.action.scene.type.previous"},
};

bool MacroConditionScene::CheckCondition()
{
	bool ret = false;
	if (_type == SceneType::CURRENT) {
		obs_source_t *rawScene = obs_frontend_get_current_scene();
		OBSWeakSource currentScene =
			obs_source_get_weak_source(rawScene);
		ret = currentScene == _scene;
		obs_weak_source_release(currentScene);
		obs_source_release(rawScene);
	} else {
		ret = switcher->previousScene == _scene;
	}
	return ret;
}

bool MacroConditionScene::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "scene", GetWeakSourceName(_scene).c_str());
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	return true;
}

bool MacroConditionScene::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene = GetWeakSourceByName(obs_data_get_string(obj, "scene"));
	_type = static_cast<SceneType>(obs_data_get_int(obj, "type"));
	return true;
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : sceneTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneEdit::MacroConditionSceneEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScene> entryData)
{
	_sceneSelection = new QComboBox();
	_sceneType = new QComboBox();

	QWidget::connect(_sceneSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SceneChanged(const QString &)));
	QWidget::connect(_sceneType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));

	AdvSceneSwitcher::populateSceneSelection(_sceneSelection);
	populateTypeSelection(_sceneType);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _sceneSelection},
		{"{{sceneType}}", _sceneType},
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

void MacroConditionSceneEdit::TypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_type = static_cast<SceneType>(value);
}

void MacroConditionSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sceneSelection->setCurrentText(
		GetWeakSourceName(_entryData->_scene).c_str());
	_sceneType->setCurrentIndex(static_cast<int>(_entryData->_type));
}
