#include "macro-condition-edit.hpp"
#include "macro-condition-scene-visibility.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"

#include <regex>

const std::string MacroConditionSceneVisibility::id = "scene_visibility";

bool MacroConditionSceneVisibility::_registered =
	MacroConditionFactory::Register(
		MacroConditionSceneVisibility::id,
		{MacroConditionSceneVisibility::Create,
		 MacroConditionSceneVisibilityEdit::Create,
		 "AdvSceneSwitcher.condition.sceneVisibility"});

static std::map<MacroConditionSceneVisibility::Condition, std::string>
	conditionTypes = {
		{MacroConditionSceneVisibility::Condition::SHOWN,
		 "AdvSceneSwitcher.condition.sceneVisibility.type.shown"},
		{MacroConditionSceneVisibility::Condition::HIDDEN,
		 "AdvSceneSwitcher.condition.sceneVisibility.type.hidden"},
		{MacroConditionSceneVisibility::Condition::CHANGED,
		 "AdvSceneSwitcher.condition.sceneVisibility.type.changed"},
};

bool areAllSceneItemsShown(const std::vector<obs_scene_item *> &items)
{
	bool ret = true;
	for (auto item : items) {
		if (!obs_sceneitem_visible(item)) {
			ret = false;
		}
		obs_sceneitem_release(item);
	}
	return ret;
}

bool areAllSceneItemsHidden(const std::vector<obs_scene_item *> &items)
{
	bool ret = true;
	for (auto item : items) {
		if (obs_sceneitem_visible(item)) {
			ret = false;
		}
		obs_sceneitem_release(item);
	}
	return ret;
}

bool didVisibilityOfAnySceneItemsChange(
	const std::vector<obs_scene_item *> &items,
	std::vector<bool> &previousVisibility)
{
	std::vector<bool> currentVisibility;
	for (const auto &item : items) {
		currentVisibility.emplace_back(obs_sceneitem_visible(item));
	}

	bool ret = true;
	if (previousVisibility.size() != currentVisibility.size()) {
		ret = false;
	} else {
		ret = previousVisibility != currentVisibility;
	}
	previousVisibility = currentVisibility;

	for (const auto &item : items) {
		obs_sceneitem_release(item);
	}
	return ret;
}

bool MacroConditionSceneVisibility::CheckCondition()
{
	auto items = _source.GetSceneItems(_scene);
	if (items.empty()) {
		return false;
	}

	switch (_condition) {
	case Condition::SHOWN:
		return areAllSceneItemsShown(items);
	case Condition::HIDDEN:
		return areAllSceneItemsHidden(items);
	case Condition::CHANGED:
		return didVisibilityOfAnySceneItemsChange(items,
							  _previousVisibilty);
		break;
	default:
		break;
	}
	return false;
}

bool MacroConditionSceneVisibility::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_scene.Save(obj);
	_source.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));

	return true;
}

bool MacroConditionSceneVisibility::Load(obs_data_t *obj)
{
	// Convert old data format
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "source")) {
		auto sourceName = obs_data_get_string(obj, "source");
		obs_data_set_string(obj, "sceneItem", sourceName);
	}

	MacroCondition::Load(obj);
	_scene.Load(obj);
	_source.Load(obj);
	_condition = static_cast<Condition>(obs_data_get_int(obj, "condition"));
	return true;
}

std::string MacroConditionSceneVisibility::GetShortDesc() const
{
	if (_source.ToString().empty()) {
		return "";
	}
	return _scene.ToString() + " - " + _source.ToString();
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionSceneVisibilityEdit::MacroConditionSceneVisibilityEdit(
	QWidget *parent,
	std::shared_ptr<MacroConditionSceneVisibility> entryData)
	: QWidget(parent)
{
	_scenes = new SceneSelectionWidget(window(), true, false, true, true);
	_sources = new SceneItemSelectionWidget(parent);
	_conditions = new QComboBox();

	populateConditionSelection(_conditions);

	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 _sources, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_sources,
			 SIGNAL(SceneItemChanged(const SceneItemSelection &)),
			 this, SLOT(SourceChanged(const SceneItemSelection &)));
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

void MacroConditionSceneVisibilityEdit::SourceChanged(
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

void MacroConditionSceneVisibilityEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionSceneVisibilityEdit::ConditionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition =
		static_cast<MacroConditionSceneVisibility::Condition>(index);
	if (_entryData->_condition ==
	    MacroConditionSceneVisibility::Condition::CHANGED) {
		_sources->SetPlaceholderType(
			SceneItemSelectionWidget::Placeholder::ANY, false);
	} else {
		_sources->SetPlaceholderType(
			SceneItemSelectionWidget::Placeholder::ALL, false);
	}
}

void MacroConditionSceneVisibilityEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_conditions->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_scenes->SetScene(_entryData->_scene);
	if (_entryData->_condition ==
	    MacroConditionSceneVisibility::Condition::CHANGED) {
		_sources->SetPlaceholderType(
			SceneItemSelectionWidget::Placeholder::ANY, false);
	} else {
		_sources->SetPlaceholderType(
			SceneItemSelectionWidget::Placeholder::ALL, false);
	}
	_sources->SetSceneItem(_entryData->_source);
}
