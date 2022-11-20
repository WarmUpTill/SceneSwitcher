#include "macro-action-scene-visibility.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

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
	{SceneVisibilityAction::TOGGLE,
	 "AdvSceneSwitcher.action.sceneVisibility.type.toggle"},
};

const static std::map<SceneItemSourceType, std::string> sourceItemSourceTypes = {
	{SceneItemSourceType::SOURCE,
	 "AdvSceneSwitcher.action.sceneVisibility.type.source"},
	{SceneItemSourceType::SOURCE_GROUP,
	 "AdvSceneSwitcher.action.sceneVisibility.type.sourceGroup"},
};

struct VisibilityData {
	std::string name;
	SceneVisibilityAction action;
};

static void setSceneItemVisiblity(obs_sceneitem_t *item,
				  SceneVisibilityAction action)
{
	switch (action) {
	case SceneVisibilityAction::SHOW:
		obs_sceneitem_set_visible(item, true);
		break;
	case SceneVisibilityAction::HIDE:
		obs_sceneitem_set_visible(item, false);
		break;
	case SceneVisibilityAction::TOGGLE:
		obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
		break;
	}
}

static bool visibilitySourceTypeEnum(obs_scene_t *, obs_sceneitem_t *item,
				     void *ptr)
{
	VisibilityData *vInfo = reinterpret_cast<VisibilityData *>(ptr);

	auto sourceTypeName = obs_source_get_display_name(
		obs_source_get_id(obs_sceneitem_get_source(item)));
	if (sourceTypeName && vInfo->name == sourceTypeName) {
		setSceneItemVisiblity(item, vInfo->action);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, visibilitySourceTypeEnum, ptr);
	}

	return true;
}

bool MacroActionSceneVisibility::PerformAction()
{
	switch (_sourceType) {
	case SceneItemSourceType::SOURCE: {
		auto items = _source.GetSceneItems(_scene);
		for (auto item : items) {
			setSceneItemVisiblity(item, _action);
			obs_sceneitem_release(item);
		}
		break;
	}
	case SceneItemSourceType::SOURCE_GROUP: {
		auto s = obs_weak_source_get_source(_scene.GetScene());
		auto scene = obs_scene_from_source(s);
		VisibilityData vInfo = {_sourceGroup, _action};
		obs_scene_enum_items(scene, visibilitySourceTypeEnum, &vInfo);
		obs_source_release(s);
		break;
	}
	default:
		break;
	}

	return true;
}

void MacroActionSceneVisibility::LogAction()
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		if (_sourceType == SceneItemSourceType::SOURCE) {
			vblog(LOG_INFO,
			      "performed visibility action \"%s\" for source \"%s\" on scene \"%s\"",
			      it->second.c_str(), _source.ToString().c_str(),
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
		_source.Save(obj);
	} else {
		obs_data_set_string(obj, "sourceGroup", _sourceGroup.c_str());
	}
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "sourceType", static_cast<int>(_sourceType));
	return true;
}

bool MacroActionSceneVisibility::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
		obs_data_set_string(obj, "sourceGroup", sourceName);
	}

	MacroAction::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_sourceType = static_cast<SceneItemSourceType>(
		obs_data_get_int(obj, "sourceType"));
	_sourceGroup = obs_data_get_string(obj, "sourceGroup");
	_action = static_cast<SceneVisibilityAction>(
		obs_data_get_int(obj, "action"));
	return true;
}

std::string MacroActionSceneVisibility::GetShortDesc()
{
	if (_sourceType == SceneItemSourceType::SOURCE &&
	    !_source.ToString().empty()) {
		return _scene.ToString() + " - " + _source.ToString();
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
	_scenes = new SceneSelectionWidget(window(), true, false, true, true);
	_sourceTypes = new QComboBox();
	_sources = new SceneItemSelectionWidget(parent);
	_sourceGroups = new QComboBox();
	_actions = new QComboBox();

	populateSourceItemTypeSelection(_sourceTypes);
	populateSourceGroupSelection(_sourceGroups);
	populateActionSelection(_actions);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sourceTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceTypeChanged(int)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
	QWidget::connect(_sourceGroups,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceGroupChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},   {"{{sourceTypes}}", _sourceTypes},
		{"{{sources}}", _sources}, {"{{sourceGroups}}", _sourceGroups},
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
	_sources->SetSceneItem((_entryData->_source));
	_sourceGroups->setCurrentText(
		QString::fromStdString(_entryData->_sourceGroup));
	SetWidgetVisibility();
}

void MacroActionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
}

void MacroActionSceneVisibilityEdit::SourceTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_sourceType = static_cast<SceneItemSourceType>(value);
	SetWidgetVisibility();
}

void MacroActionSceneVisibilityEdit::SourceChanged(
	const SceneItemSelection &item)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = item;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionSceneVisibilityEdit::SourceGroupChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (text == obs_module_text("AdvSceneSwitcher.selectItem")) {
		_entryData->_sourceGroup = "";
	} else {
		_entryData->_sourceGroup = text.toStdString();
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

void MacroActionSceneVisibilityEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}
	_sources->setVisible(_entryData->_sourceType ==
			     SceneItemSourceType::SOURCE);
	_sourceGroups->setVisible(_entryData->_sourceType ==
				  SceneItemSourceType::SOURCE_GROUP);
	adjustSize();
}
