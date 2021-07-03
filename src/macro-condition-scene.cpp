#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionScene::id = "scene";

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	MacroConditionScene::id,
	{MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	 "AdvSceneSwitcher.condition.scene"});

static std::map<SceneType, std::string> sceneTypes = {
	{SceneType::CURRENT, "AdvSceneSwitcher.condition.scene.type.current"},
	{SceneType::PREVIOUS, "AdvSceneSwitcher.condition.scene.type.previous"},
};

bool MacroConditionScene::CheckCondition()
{
	bool sceneMatch = false;
	if (_type == SceneType::CURRENT) {
		sceneMatch = switcher->currentScene == _scene.GetScene(false);
	} else {
		sceneMatch = switcher->previousScene == _scene.GetScene(false);
	}

	return sceneMatch;
}

bool MacroConditionScene::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	return true;
}

bool MacroConditionScene::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	_type = static_cast<SceneType>(obs_data_get_int(obj, "type"));
	return true;
}

std::string MacroConditionScene::GetShortDesc()
{
	return _scene.ToString();
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : sceneTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneEdit::MacroConditionSceneEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScene> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, false, false);
	_sceneType = new QComboBox();

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sceneType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));

	populateTypeSelection(_sceneType);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sceneType}}", _sceneType},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.scene.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
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

	_scenes->SetScene(_entryData->_scene);
	_sceneType->setCurrentIndex(static_cast<int>(_entryData->_type));
}
