#include "macro-condition-edit.hpp"
#include "macro-condition-scene.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

const std::string MacroConditionScene::id = "scene";

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	MacroConditionScene::id,
	{MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	 "AdvSceneSwitcher.condition.scene"});

static std::map<SceneType, std::string> sceneTypes = {
	{SceneType::CURRENT, "AdvSceneSwitcher.condition.scene.type.current"},
	{SceneType::PREVIOUS, "AdvSceneSwitcher.condition.scene.type.previous"},
	{SceneType::CHANGED, "AdvSceneSwitcher.condition.scene.type.changed"},
	{SceneType::NOTCHANGED,
	 "AdvSceneSwitcher.condition.scene.type.notChanged"},
};

bool MacroConditionScene::CheckCondition()
{
	bool sceneChanged = _lastSceneChangeTime !=
			    switcher->lastSceneChangeTime;
	if (sceneChanged) {
		_lastSceneChangeTime = switcher->lastSceneChangeTime;
	}

	switch (_type) {
	case SceneType::CURRENT:
		if (_useTransitionTargetScene) {
			auto current = obs_frontend_get_current_scene();
			auto weak = obs_source_get_weak_source(current);
			bool match = weak == _scene.GetScene(false);
			obs_weak_source_release(weak);
			obs_source_release(current);
			return match;
		}
		return switcher->currentScene == _scene.GetScene(false);
	case SceneType::PREVIOUS:
		if (switcher->anySceneTransitionStarted() &&
		    _useTransitionTargetScene) {
			return switcher->currentScene == _scene.GetScene(false);
		}
		return switcher->previousScene == _scene.GetScene(false);
	case SceneType::CHANGED:
		return sceneChanged;
	case SceneType::NOTCHANGED:
		return !sceneChanged;
	default:
		break;
	}

	return false;
}

bool MacroConditionScene::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_bool(obj, "useTransitionTargetScene",
			  _useTransitionTargetScene);
	return true;
}

bool MacroConditionScene::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	_type = static_cast<SceneType>(obs_data_get_int(obj, "type"));
	if (obs_data_has_user_value(obj, "waitForTransition")) {
		_useTransitionTargetScene =
			!obs_data_get_bool(obj, "waitForTransition");
	} else {
		_useTransitionTargetScene =
			obs_data_get_bool(obj, "useTransitionTargetScene");
	}
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
	_useTransitionTargetScene = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.scene.currentSceneTransitionBehaviour"));

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sceneType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_useTransitionTargetScene, SIGNAL(stateChanged(int)),
			 this, SLOT(UseTransitionTargetSceneChanged(int)));

	populateTypeSelection(_sceneType);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sceneType}}", _sceneType},
		{"{{useTransitionTargetScene}}", _useTransitionTargetScene},
	};
	QHBoxLayout *line1Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.scene.entry.line1"),
		line1Layout, widgetPlaceholders);
	QHBoxLayout *line2Layout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.condition.scene.entry.line2"),
		line2Layout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(line1Layout);
	mainLayout->addLayout(line2Layout);
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
	SetWidgetVisibility();
}

void MacroConditionSceneEdit::UseTransitionTargetSceneChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_useTransitionTargetScene = state;
}

void MacroConditionSceneEdit::SetWidgetVisibility()
{
	_scenes->setVisible(_entryData->_type == SceneType::CURRENT ||
			    _entryData->_type == SceneType::PREVIOUS);
	_useTransitionTargetScene->setVisible(
		_entryData->_type == SceneType::CURRENT ||
		_entryData->_type == SceneType::PREVIOUS);

	if (_entryData->_type == SceneType::PREVIOUS) {
		_useTransitionTargetScene->setText(obs_module_text(
			"AdvSceneSwitcher.condition.scene.previousSceneTransitionBehaviour"));
	}
	if (_entryData->_type == SceneType::CURRENT) {
		_useTransitionTargetScene->setText(obs_module_text(
			"AdvSceneSwitcher.condition.scene.currentSceneTransitionBehaviour"));
	}
	adjustSize();
}

void MacroConditionSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sceneType->setCurrentIndex(static_cast<int>(_entryData->_type));
	_useTransitionTargetScene->setChecked(
		_entryData->_useTransitionTargetScene);
	SetWidgetVisibility();
}
