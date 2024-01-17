#include "macro-condition-scene.hpp"
#include "scene-switch-helpers.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionScene::id =
	MacroCondition::GetDefaultID().data();

bool MacroConditionScene::_registered = MacroConditionFactory::Register(
	MacroConditionScene::id,
	{MacroConditionScene::Create, MacroConditionSceneEdit::Create,
	 "AdvSceneSwitcher.condition.scene"});

const static std::map<MacroConditionScene::Type, std::string> sceneTypes = {
	{MacroConditionScene::Type::CURRENT,
	 "AdvSceneSwitcher.condition.scene.type.current"},
	{MacroConditionScene::Type::PREVIOUS,
	 "AdvSceneSwitcher.condition.scene.type.previous"},
	{MacroConditionScene::Type::PREVIEW,
	 "AdvSceneSwitcher.condition.scene.type.preview"},
	{MacroConditionScene::Type::CHANGED,
	 "AdvSceneSwitcher.condition.scene.type.changed"},
	{MacroConditionScene::Type::NOT_CHANGED,
	 "AdvSceneSwitcher.condition.scene.type.notChanged"},
	{MacroConditionScene::Type::CURRENT_PATTERN,
	 "AdvSceneSwitcher.condition.scene.type.currentPattern"},
	{MacroConditionScene::Type::PREVIOUS_PATTERN,
	 "AdvSceneSwitcher.condition.scene.type.previousPattern"},
	{MacroConditionScene::Type::PREVIEW_PATTERN,
	 "AdvSceneSwitcher.condition.scene.type.previewPattern"},
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

static OBSWeakSource getCurrentSceneHelper(bool useTransitionTargetScene)
{
	if (useTransitionTargetScene) {
		auto current = obs_frontend_get_current_scene();
		auto weak = obs_source_get_weak_source(current);
		obs_weak_source_release(weak);
		obs_source_release(current);
		return weak;
	}
	return GetCurrentScene();
}

static OBSWeakSource getPreviousSceneHelper(bool useTransitionTargetScene)
{
	if (AnySceneTransitionStarted() && useTransitionTargetScene) {
		return GetCurrentScene();
	}
	return GetPreviousScene();
}

bool MacroConditionScene::CheckCondition()
{
	bool sceneChanged = _lastSceneChangeTime != GetLastSceneChangeTime();
	if (sceneChanged) {
		_lastSceneChangeTime = GetLastSceneChangeTime();
	}

	switch (_type) {
	case Type::CURRENT: {
		auto scene = getCurrentSceneHelper(_useTransitionTargetScene);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("current", GetWeakSourceName(scene));
		return scene == _scene.GetScene(false);
	}
	case Type::PREVIOUS: {
		auto scene = getPreviousSceneHelper(_useTransitionTargetScene);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("previous", GetWeakSourceName(scene));
		return scene == _scene.GetScene(false);
	}
	case Type::PREVIEW: {
		OBSSourceAutoRelease source =
			obs_frontend_get_current_preview_scene();
		OBSWeakSourceAutoRelease scene =
			obs_source_get_weak_source(source);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("preview", GetWeakSourceName(scene));
		return scene == _scene.GetScene(false);
	}
	case Type::CHANGED:
		SetVariableValue(GetWeakSourceName(GetCurrentScene()));
		SetTempVarValue("current",
				GetWeakSourceName(GetCurrentScene()));
		return sceneChanged;
	case Type::NOT_CHANGED:
		SetVariableValue(GetWeakSourceName(GetCurrentScene()));
		SetTempVarValue("current",
				GetWeakSourceName(GetCurrentScene()));
		return !sceneChanged;
	case Type::CURRENT_PATTERN: {
		auto scene = getCurrentSceneHelper(_useTransitionTargetScene);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("current", GetWeakSourceName(scene));
		return sceneNameMatchesRegex(scene, _pattern);
	}
	case Type::PREVIOUS_PATTERN: {
		auto scene = getPreviousSceneHelper(_useTransitionTargetScene);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("previous", GetWeakSourceName(scene));
		return sceneNameMatchesRegex(scene, _pattern);
	}
	case Type::PREVIEW_PATTERN: {
		OBSSourceAutoRelease source =
			obs_frontend_get_current_preview_scene();
		OBSWeakSourceAutoRelease scene =
			obs_source_get_weak_source(source);
		SetVariableValue(GetWeakSourceName(scene));
		SetTempVarValue("preview", GetWeakSourceName(scene));
		return sceneNameMatchesRegex(scene.Get(), _pattern);
	}
	}

	return false;
}

bool MacroConditionScene::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_int(obj, "type", static_cast<int>(_type));
	obs_data_set_string(obj, "pattern", _pattern.c_str());
	obs_data_set_bool(obj, "useTransitionTargetScene",
			  _useTransitionTargetScene);
	obs_data_set_int(obj, "version", 1);
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

	// TODO: Remove fallback in future version
	if (!obs_data_has_user_value(obj, "version")) {
		enum {
			CURRENT,
			PREVIOUS,
			CHANGED,
			NOT_CHANGED,
			CURRENT_PATTERN,
			PREVIOUS_PATTERN,
		};

		int oldType = obs_data_get_int(obj, "type");
		switch (oldType) {
		case CURRENT:
			_type = Type::CURRENT;
			break;
		case PREVIOUS:
			_type = Type::PREVIOUS;
			break;
		case CHANGED:
			_type = Type::CHANGED;
			break;
		case NOT_CHANGED:
			_type = Type::NOT_CHANGED;
			break;
		case CURRENT_PATTERN:
			_type = Type::CURRENT_PATTERN;
			break;
		case PREVIOUS_PATTERN:
			_type = Type::PREVIOUS_PATTERN;
			break;
		default:
			blog(LOG_WARNING,
			     "failed to convert scene condition type (%d)",
			     oldType);
			_type = Type::CURRENT;
			break;
		}
	}
	return true;
}

std::string MacroConditionScene::GetShortDesc() const
{
	if (_type == Type::CURRENT || _type == Type::PREVIOUS) {
		return _scene.ToString();
	}
	return "";
}

void MacroConditionScene::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_type) {
	case Type::CURRENT:
	case Type::CHANGED:
	case Type::NOT_CHANGED:
	case Type::CURRENT_PATTERN:
		AddTempvar("current",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.scene.current"));
		break;
	case Type::PREVIOUS:
	case Type::PREVIOUS_PATTERN:
		AddTempvar("previous",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.scene.previous"));
		break;
	case Type::PREVIEW:
	case Type::PREVIEW_PATTERN:
		AddTempvar("preview",
			   obs_module_text(
				   "AdvSceneSwitcher.tempVar.scene.preview"));
		break;
	default:
		break;
	}
}

void MacroConditionScene::SetType(const Type &type)
{
	_type = type;
	SetupTempVars();
}

static inline void populateTypeSelection(QComboBox *list)
{
	for (const auto &[id, name] : sceneTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(id));
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
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.scene.entry.line1"),
		line1Layout, widgetPlaceholders);
	QHBoxLayout *line2Layout = new QHBoxLayout;
	PlaceWidgets(
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

	auto lock = LockContext();
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneEdit::TypeChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetType(static_cast<MacroConditionScene::Type>(
		_sceneType->itemData(index).toInt()));
	SetWidgetVisibility();
}

void MacroConditionSceneEdit::PatternChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_pattern = _pattern->text().toStdString();
}

void MacroConditionSceneEdit::UseTransitionTargetSceneChanged(int state)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_useTransitionTargetScene = state;
}

void MacroConditionSceneEdit::SetWidgetVisibility()
{
	_scenes->setVisible(
		_entryData->GetType() == MacroConditionScene::Type::CURRENT ||
		_entryData->GetType() == MacroConditionScene::Type::PREVIOUS ||
		_entryData->GetType() == MacroConditionScene::Type::PREVIEW);
	_useTransitionTargetScene->setVisible(
		_entryData->GetType() == MacroConditionScene::Type::CURRENT ||
		_entryData->GetType() == MacroConditionScene::Type::PREVIOUS ||
		_entryData->GetType() ==
			MacroConditionScene::Type::CURRENT_PATTERN ||
		_entryData->GetType() ==
			MacroConditionScene::Type::PREVIOUS_PATTERN);
	_pattern->setVisible(
		_entryData->GetType() ==
			MacroConditionScene::Type::CURRENT_PATTERN ||
		_entryData->GetType() ==
			MacroConditionScene::Type::PREVIOUS_PATTERN ||
		_entryData->GetType() ==
			MacroConditionScene::Type::PREVIEW_PATTERN);

	if (_entryData->GetType() == MacroConditionScene::Type::PREVIOUS ||
	    _entryData->GetType() ==
		    MacroConditionScene::Type::PREVIOUS_PATTERN) {
		_useTransitionTargetScene->setText(obs_module_text(
			"AdvSceneSwitcher.condition.scene.previousSceneTransitionBehaviour"));
	}
	if (_entryData->GetType() == MacroConditionScene::Type::CURRENT ||
	    _entryData->GetType() ==
		    MacroConditionScene::Type::CURRENT_PATTERN) {
		_useTransitionTargetScene->setText(obs_module_text(
			"AdvSceneSwitcher.condition.scene.currentSceneTransitionBehaviour"));
	}
	adjustSize();
	updateGeometry();
}

void MacroConditionSceneEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_scenes->SetScene(_entryData->_scene);
	_sceneType->setCurrentIndex(
		_sceneType->findData(static_cast<int>(_entryData->GetType())));
	_pattern->setText(QString::fromStdString(_entryData->_pattern));
	_useTransitionTargetScene->setChecked(
		_entryData->_useTransitionTargetScene);
	SetWidgetVisibility();
}

} // namespace advss
