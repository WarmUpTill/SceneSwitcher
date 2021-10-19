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
		if (_waitForTransition) {
			return switcher->currentScene == _scene.GetScene(false);
		} else {

			bool match = false;
			auto current = obs_frontend_get_current_scene();
			auto weak = obs_source_get_weak_source(current);
			match = weak == _scene.GetScene(false);
			obs_weak_source_release(weak);
			obs_source_release(current);
			return match;
		}
	case SceneType::PREVIOUS:
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
	obs_data_set_bool(obj, "waitForTransition", _waitForTransition);
	return true;
}

bool MacroConditionScene::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	_type = static_cast<SceneType>(obs_data_get_int(obj, "type"));
	if (!obs_data_has_user_value(obj, "waitForTransition")) {
		_waitForTransition = true;
	} else {
		_waitForTransition =
			obs_data_get_bool(obj, "waitForTransition");
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
	_waitForTransition = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.scene.waitForTransition"));

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sceneType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_waitForTransition, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitForTransitionChanged(int)));

	populateTypeSelection(_sceneType);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sceneType}}", _sceneType},
		{"{{waitForTransition}}", _waitForTransition},
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
	SetWidgetVisibility();
}

void MacroConditionSceneEdit::WaitForTransitionChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_waitForTransition = state;
}

void MacroConditionSceneEdit::SetWidgetVisibility()
{
	_scenes->setVisible(_entryData->_type == SceneType::CURRENT ||
			    _entryData->_type == SceneType::PREVIOUS);
	_waitForTransition->setVisible(_entryData->_type == SceneType::CURRENT);
}

void MacroConditionSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sceneType->setCurrentIndex(static_cast<int>(_entryData->_type));
	_waitForTransition->setChecked(_entryData->_waitForTransition);
	SetWidgetVisibility();
}
