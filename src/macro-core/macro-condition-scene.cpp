#include "macro-condition-edit.hpp"
#include "macro-condition-scene.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

const std::string MacroConditionScene::id = "scene";

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	MacroConditionScene::id,
	{MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	 "AdvSceneSwitcher.condition.scene"});

static std::map<MacroConditionScene::Type, std::string> sceneTypes = {
	{MacroConditionScene::Type::CURRENT,
	 "AdvSceneSwitcher.condition.scene.type.current"},
	{MacroConditionScene::Type::PREVIOUS,
	 "AdvSceneSwitcher.condition.scene.type.previous"},
	{MacroConditionScene::Type::CHANGED,
	 "AdvSceneSwitcher.condition.scene.type.changed"},
	{MacroConditionScene::Type::NOT_CHANGED,
	 "AdvSceneSwitcher.condition.scene.type.notChanged"},
	{MacroConditionScene::Type::CURRENT_PATTERN,
	 "AdvSceneSwitcher.condition.scene.type.currentPattern"},
	{MacroConditionScene::Type::PREVIOUS_PATTERN,
	 "AdvSceneSwitcher.condition.scene.type.previousPattern"},
};

static bool sceneNameMatchesRegex(const OBSWeakSource &scene,
				  const std::string &pattern)
{
	auto regex = QRegularExpression(QRegularExpression::anchoredPattern(
		QString::fromStdString(pattern)));
	if (!regex.isValid()) {
		return false;
	}

	auto match =
		regex.match(QString::fromStdString(GetWeakSourceName(scene)));
	return match.hasMatch();
}

bool MacroConditionScene::CheckCondition()
{
	bool sceneChanged = _lastSceneChangeTime !=
			    switcher->lastSceneChangeTime;
	if (sceneChanged) {
		_lastSceneChangeTime = switcher->lastSceneChangeTime;
	}

	switch (_type) {
	case Type::CURRENT:
		if (_useTransitionTargetScene) {
			auto current = obs_frontend_get_current_scene();
			auto weak = obs_source_get_weak_source(current);
			bool match = weak == _scene.GetScene(false);
			obs_weak_source_release(weak);
			obs_source_release(current);
			return match;
		}
		return switcher->currentScene == _scene.GetScene(false);
	case Type::PREVIOUS:
		if (switcher->anySceneTransitionStarted() &&
		    _useTransitionTargetScene) {
			return switcher->currentScene == _scene.GetScene(false);
		}
		return switcher->previousScene == _scene.GetScene(false);
	case Type::CHANGED:
		return sceneChanged;
	case Type::NOT_CHANGED:
		return !sceneChanged;
	case Type::CURRENT_PATTERN:
		return sceneNameMatchesRegex(switcher->currentScene, _pattern);
	case Type::PREVIOUS_PATTERN:
		return sceneNameMatchesRegex(switcher->previousScene, _pattern);
	}

	return false;
}

bool MacroConditionScene::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_string(obj, "pattern", _pattern.c_str());
	obs_data_set_bool(obj, "useTransitionTargetScene",
			  _useTransitionTargetScene);
	return true;
}

bool MacroConditionScene::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	_type = static_cast<Type>(obs_data_get_int(obj, "type"));
	_pattern = obs_data_get_string(obj, "pattern");
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
	if (_type == Type::CURRENT || _type == Type::PREVIOUS) {
		return _scene.ToString();
	}
	return "";
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (auto entry : sceneTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneEdit::MacroConditionSceneEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScene> entryData)
	: QWidget(parent),
	  _scenes(new SceneSelectionWidget(window(), true, false, false,
					   false)),
	  _sceneType(new QComboBox()),
	  _pattern(new QLineEdit()),
	  _useTransitionTargetScene(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.scene.currentSceneTransitionBehaviour")))
{
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sceneType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(TypeChanged(int)));
	QWidget::connect(_pattern, SIGNAL(editingFinished()), this,
			 SLOT(PatternChanged()));
	QWidget::connect(_useTransitionTargetScene, SIGNAL(stateChanged(int)),
			 this, SLOT(UseTransitionTargetSceneChanged(int)));

	populateTypeSelection(_sceneType);

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sceneType}}", _sceneType},
		{"{{pattern}}", _pattern},
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
	_entryData->_type = static_cast<MacroConditionScene::Type>(value);
	SetWidgetVisibility();
}

void MacroConditionSceneEdit::PatternChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_pattern = _pattern->text().toStdString();
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
	_scenes->setVisible(
		_entryData->_type == MacroConditionScene::Type::CURRENT ||
		_entryData->_type == MacroConditionScene::Type::PREVIOUS);
	_useTransitionTargetScene->setVisible(
		_entryData->_type == MacroConditionScene::Type::CURRENT ||
		_entryData->_type == MacroConditionScene::Type::PREVIOUS);
	_pattern->setVisible(
		_entryData->_type ==
			MacroConditionScene::Type::CURRENT_PATTERN ||
		_entryData->_type ==
			MacroConditionScene::Type::PREVIOUS_PATTERN);

	if (_entryData->_type == MacroConditionScene::Type::PREVIOUS) {
		_useTransitionTargetScene->setText(obs_module_text(
			"AdvSceneSwitcher.condition.scene.previousSceneTransitionBehaviour"));
	}
	if (_entryData->_type == MacroConditionScene::Type::CURRENT) {
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
	_pattern->setText(QString::fromStdString(_entryData->_pattern));
	_useTransitionTargetScene->setChecked(
		_entryData->_useTransitionTargetScene);
	SetWidgetVisibility();
}
