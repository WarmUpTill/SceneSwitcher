#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-scene-visibility.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionSceneVisibility::id = "scene_visibility";

bool MacroConditionSceneVisibility::_registered =
	MacroConditionFactory::Register(
		MacroConditionSceneVisibility::id,
		{MacroConditionSceneVisibility::Create,
		 MacroConditionSceneVisibilityEdit::Create,
		 "AdvSceneSwitcher.condition.sceneVisibility"});

static std::map<SceneVisibilityCondition, std::string>
	SceneVisibilityConditionTypes = {
		{SceneVisibilityCondition::SHOWN,
		 "AdvSceneSwitcher.condition.sceneVisibility.type.shown"},
		{SceneVisibilityCondition::HIDDEN,
		 "AdvSceneSwitcher.condition.sceneVisibility.type.hidden"},
};

struct VisibilityData {
	std::string name;
	bool visible;
	bool result;
};

static bool visibilityEnum(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	VisibilityData *vInfo = reinterpret_cast<VisibilityData *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (vInfo->name == sourceName &&
	    obs_sceneitem_visible(item) == vInfo->visible) {
		vInfo->result = true;
		return false;
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, visibilityEnum, ptr);
	}

	return true;
}

bool MacroConditionSceneVisibility::CheckCondition()
{
	if (!_source) {
		return false;
	}

	auto s = obs_weak_source_get_source(_scene.GetScene());
	auto scene = obs_scene_from_source(s);
	auto sourceName = GetWeakSourceName(_source);
	VisibilityData data = {sourceName,
			       _condition == SceneVisibilityCondition::SHOWN,
			       false};
	obs_scene_enum_items(scene, visibilityEnum, &data);
	obs_source_release(s);
	return data.result;
}

bool MacroConditionSceneVisibility::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());

	obs_data_set_int(obj, "condition", static_cast<int>(_condition));

	return true;
}

bool MacroConditionSceneVisibility::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_scene.Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_condition = static_cast<SceneVisibilityCondition>(
		obs_data_get_int(obj, "condition"));
	return true;
}

std::string MacroConditionSceneVisibility::GetShortDesc()
{
	if (_source) {
		return _scene.ToString() + " - " + GetWeakSourceName(_source);
	}
	return "";
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : SceneVisibilityConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneVisibilityEdit::MacroConditionSceneVisibilityEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneVisibility> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), false, true, true);
	_sources = new QComboBox();
	_sources->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_conditions = new QComboBox();

	populateConditionSelection(_conditions);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SourceChanged(const QString &)));
	QWidget::connect(_conditions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{conditions}}", _conditions},
	};
	QHBoxLayout *mainLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.sceneVisibility.entry"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionSceneVisibilityEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_source = GetWeakSourceByQString(text);
}

void MacroConditionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
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
	_sources->adjustSize();
}

void MacroConditionSceneVisibilityEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<SceneVisibilityCondition>(index);
}

void MacroConditionSceneVisibilityEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_scenes->SetScene(_entryData->_scene);
	populateSceneItemSelection(_sources, _entryData->_scene);
	_sources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
}
