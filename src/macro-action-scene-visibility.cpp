#include "headers/macro-action-scene-visibility.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionSceneVisibility::id = "scene_visibility";

bool MacroActionSceneVisibility::_registered = MacroActionFactory::Register(
	MacroActionSceneVisibility::id,
	{MacroActionSceneVisibility::Create,
	 MacroActionSceneVisibilityEdit::Create,
	 "AdvSceneSwitcher.action.sceneVisibility"});

const static std::map<SceneVisibilityAction, std::string> actionTypes = {
	{SceneVisibilityAction::SHOW,
	 "AdvSceneSwitcher.action.sceneVisibility.type.show"},
	{SceneVisibilityAction::HIDE,
	 "AdvSceneSwitcher.action.sceneVisibility.type.hide"},
};

const static std::map<SceneItemSourceType, std::string> sourceItemSourceTypes = {
	{SceneItemSourceType::SOURCE,
	 "AdvSceneSwitcher.action.sceneVisibility.type.source"},
	{SceneItemSourceType::SOURCE_GROUP,
	 "AdvSceneSwitcher.action.sceneVisibility.type.sourceGroup"},
};

struct VisibilityData {
	std::string name;
	bool visible;
};

static bool visibilitySourceEnum(obs_scene_t *, obs_sceneitem_t *item,
				 void *ptr)
{
	VisibilityData *vInfo = reinterpret_cast<VisibilityData *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (vInfo->name == sourceName) {
		obs_sceneitem_set_visible(item, vInfo->visible);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, visibilitySourceEnum, ptr);
	}

	return true;
}

static bool visibilitySourceTypeEnum(obs_scene_t *, obs_sceneitem_t *item,
				     void *ptr)
{
	VisibilityData *vInfo = reinterpret_cast<VisibilityData *>(ptr);

	auto sourceTypeName = obs_source_get_display_name(
		obs_source_get_id(obs_sceneitem_get_source(item)));
	if (vInfo->name == sourceTypeName) {
		obs_sceneitem_set_visible(item, vInfo->visible);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, visibilitySourceTypeEnum, ptr);
	}

	return true;
}

bool MacroActionSceneVisibility::PerformAction()
{
	auto s = obs_weak_source_get_source(_scene.GetScene());
	auto scene = obs_scene_from_source(s);
	auto sourceName = GetWeakSourceName(_source);
	VisibilityData vInfo = {"", _action == SceneVisibilityAction::SHOW};

	switch (_sourceType) {
	case SceneItemSourceType::SOURCE:
		vInfo.name = sourceName;
		obs_scene_enum_items(scene, visibilitySourceEnum, &vInfo);
		break;
	case SceneItemSourceType::SOURCE_GROUP:
		vInfo.name = _sourceGroup;
		obs_scene_enum_items(scene, visibilitySourceTypeEnum, &vInfo);
		break;
	default:
		break;
	}

	obs_source_release(s);
	return true;
}

void MacroActionSceneVisibility::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		if (_sourceType == SceneItemSourceType::SOURCE) {
			vblog(LOG_INFO,
			      "performed visibility action \"%s\" for source \"%s\" on scene \"%s\"",
			      it->second.c_str(),
			      GetWeakSourceName(_source).c_str(),
			      _scene.ToString().c_str());
		} else {
			vblog(LOG_INFO,
			      "performed visibility action \"%s\" for any source type \"%s\" on scene \"%s\"",
			      it->second.c_str(), _sourceGroup.c_str(),
			      _scene.ToString().c_str());
		}
	} else {
		blog(LOG_WARNING, "ignored unknown SceneVisibility action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneVisibility::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_scene.Save(obj);
	if (_sourceType == SceneItemSourceType::SOURCE) {
		obs_data_set_string(obj, "source",
				    GetWeakSourceName(_source).c_str());
	} else {
		obs_data_set_string(obj, "source", _sourceGroup.c_str());
	}
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "sourceType", static_cast<int>(_sourceType));
	return true;
}

bool MacroActionSceneVisibility::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_scene.Load(obj);
	_sourceType = static_cast<SceneItemSourceType>(
		obs_data_get_int(obj, "sourceType"));
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_sourceGroup = sourceName;
	_action = static_cast<SceneVisibilityAction>(
		obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionSceneVisibility::GetShortDesc()
{
	if (_sourceType == SceneItemSourceType::SOURCE && _source) {
		return _scene.ToString() + " - " + GetWeakSourceName(_source);
	}
	if (_sourceType == SceneItemSourceType::SOURCE_GROUP &&
	    !_sourceGroup.empty()) {
		return _scene.ToString() + " - " +
		       obs_module_text(
			       "AdvSceneSwitcher.action.sceneVisibility.type.sourceGroup") +
		       " " + _sourceGroup;
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateSourceItemTypeSelection(QComboBox *list)
{
	for (auto entry : sourceItemSourceTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneVisibilityEdit::MacroActionSceneVisibilityEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneVisibility> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, true, true);
	_sourceTypes = new QComboBox();
	_sources = new QComboBox();
	_actions = new QComboBox();

	populateSourceItemTypeSelection(_sourceTypes);
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sourceTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceTypeChanged(int)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sourceTypes}}", _sourceTypes},
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.sceneVisibility.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionSceneVisibilityEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_sourceTypes->setCurrentIndex(
		static_cast<int>(_entryData->_sourceType));
	_scenes->SetScene(_entryData->_scene);
	if (_entryData->_sourceType == SceneItemSourceType::SOURCE) {
		populateSceneItemSelection(_sources, _entryData->_scene);
		_sources->setCurrentText(
			GetWeakSourceName(_entryData->_source).c_str());
	} else {
		populateSourceTypeSelection(_sources);
		_sources->setCurrentText(_entryData->_sourceGroup.c_str());
	}
}

void MacroActionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_scene = s;
	}
	_sources->clear();
	populateSceneItemSelection(_sources, _entryData->_scene);
}

void MacroActionSceneVisibilityEdit::SourceTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_sourceType =
			static_cast<SceneItemSourceType>(value);
	}

	_sources->clear();
	if (_entryData->_sourceType == SceneItemSourceType::SOURCE) {
		populateSceneItemSelection(_sources, _entryData->_scene);
	} else {
		populateSourceTypeSelection(_sources);
	}
}

void MacroActionSceneVisibilityEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (_entryData->_sourceType == SceneItemSourceType::SOURCE) {
		_entryData->_source = GetWeakSourceByQString(text);
	} else {
		if (text == obs_module_text("AdvSceneSwitcher.selectItem")) {
			_entryData->_sourceGroup = "";
		} else {
			_entryData->_sourceGroup = text.toStdString();
		}
	}
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSceneVisibilityEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<SceneVisibilityAction>(value);
}
