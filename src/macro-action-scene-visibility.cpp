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

struct VisInfo {
	std::string name;
	bool visible;
};

static bool visibilityEnum(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	VisInfo *vInfo = reinterpret_cast<VisInfo *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (vInfo->name == sourceName) {
		obs_sceneitem_set_visible(item, vInfo->visible);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, visibilityEnum, ptr);
	}

	return true;
}

bool MacroActionSceneVisibility::PerformAction()
{
	auto s = obs_weak_source_get_source(_scene);
	auto scene = obs_scene_from_source(s);
	auto sourceName = GetWeakSourceName(_source);
	VisInfo vInfo = {sourceName, _action == SceneVisibilityAction::SHOW};

	switch (_action) {
	case SceneVisibilityAction::SHOW:
		obs_scene_enum_items(scene, visibilityEnum, &vInfo);
		break;
	case SceneVisibilityAction::HIDE:
		obs_scene_enum_items(scene, visibilityEnum, &vInfo);
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
		vblog(LOG_INFO,
		      "performed action \"%s\" for source \"%s\" on scene \"%s\"",
		      it->second.c_str(), GetWeakSourceName(_scene).c_str(),
		      GetWeakSourceName(_scene).c_str());
	} else {
		blog(LOG_WARNING, "ignored unknown SceneVisibility action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionSceneVisibility::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	obs_data_set_string(obj, "scene", GetWeakSourceName(_scene).c_str());
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	return true;
}

bool MacroActionSceneVisibility::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	const char *sceneName = obs_data_get_string(obj, "scene");
	_scene = GetWeakSourceByName(sceneName);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_action = static_cast<SceneVisibilityAction>(
		obs_data_get_int(obj, "action"));
	return true;
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionSceneVisibilityEdit::MacroActionSceneVisibilityEdit(
	QWidget *parent, std::shared_ptr<MacroActionSceneVisibility> entryData)
	: QWidget(parent)
{
	_scenes = new QComboBox();
	_sources = new QComboBox();
	_actions = new QComboBox();

	populateActionSelection(_actions);
	populateSceneSelection(_scenes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", _scenes},
		{"{{sources}}", _sources},
		{"{{actions}}", _actions},
	};
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.SceneVisibility.entry"),
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
	_scenes->setCurrentText(GetWeakSourceName(_entryData->_scene).c_str());
	populateSceneItemSelection(_sources, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
}

void MacroActionSceneVisibilityEdit::SceneChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		_entryData->_scene = GetWeakSourceByQString(text);
	}
	_sources->clear();
	populateSceneItemSelection(_sources, _entryData->_scene);
}

void MacroActionSceneVisibilityEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
}

void MacroActionSceneVisibilityEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<SceneVisibilityAction>(value);
}
