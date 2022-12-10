#include "scene-item-selection.hpp"

#include <obs-module.h>

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view itemSaveName = "item";
constexpr std::string_view idxSaveName = "idx";
constexpr std::string_view idxTypeSaveName = "idxType";

void SceneItemSelection::Save(obs_data_t *obj, const char *name) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	obs_data_set_int(data, idxTypeSaveName.data(),
			 static_cast<int>(_idxType));
	if (_idxType == IdxType::INDIVIDUAL) {
		obs_data_set_int(data, idxSaveName.data(), _idx);
	} else {
		obs_data_set_int(data, idxSaveName.data(), 0);
	}
	if (_type == SceneItemSelection::Type::SOURCE) {
		obs_data_set_string(data, itemSaveName.data(),
				    GetWeakSourceName(_sceneItem).c_str());
	} else {
		auto var = _variable.lock();
		if (var) {
			obs_data_set_string(data, itemSaveName.data(),
					    var->Name().c_str());
		}
	}

	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

// TODO: Remove in future version
void SceneItemSelection::Load(obs_data_t *obj, const char *name,
			      const char *typeName, const char *idxName)
{
	_type = Type::SOURCE;
	_idxType = static_cast<IdxType>(obs_data_get_int(obj, typeName));
	_idx = obs_data_get_int(obj, idxName);
	auto sceneItemName = obs_data_get_string(obj, name);
	_sceneItem = GetWeakSourceByName(sceneItemName);
}

void SceneItemSelection::Load(obs_data_t *obj, const char *name)
{
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, name)) {
		Load(obj, "sceneItem", "sceneItemTarget", "sceneItemIdx");
		return;
	}

	auto data = obs_data_get_obj(obj, name);
	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	_idxType = static_cast<IdxType>(
		obs_data_get_int(data, idxTypeSaveName.data()));
	_idx = obs_data_get_int(data, idxSaveName.data());
	const auto itemName = obs_data_get_string(data, itemSaveName.data());
	switch (_type) {
	case SceneItemSelection::Type::SOURCE:
		_sceneItem = GetWeakSourceByName(itemName);
		break;
	case SceneItemSelection::Type::VARIABLE:
		_variable = GetWeakVariableByName(itemName);
		break;
	default:
		break;
	}
	obs_data_release(data);
}

struct ItemInfo {
	std::string name;
	std::vector<obs_sceneitem_t *> items = {};
};

static bool getSceneItems(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	ItemInfo *moveInfo = reinterpret_cast<ItemInfo *>(ptr);
	auto sourceName = obs_source_get_name(obs_sceneitem_get_source(item));
	if (moveInfo->name == sourceName) {
		obs_sceneitem_addref(item);
		moveInfo->items.push_back(item);
	}

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, getSceneItems, ptr);
	}

	return true;
}

std::vector<obs_scene_item *> getSceneItemsWithName(obs_scene_t *scene,
						    std::string &name)
{
	ItemInfo itemInfo = {name};
	obs_scene_enum_items(scene, getSceneItems, &itemInfo);
	return itemInfo.items;
}

struct ItemCountData {
	std::string name;
	int count = 0;
};

static bool countSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto data = reinterpret_cast<ItemCountData *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, countSceneItem, ptr);
	}
	auto name = obs_source_get_name(obs_sceneitem_get_source(item));
	if (name == data->name) {
		data->count++;
	}
	return true;
}

int getCountOfSceneItemOccurance(const SceneSelection &s,
				 const std::string &name,
				 bool enumAllScenes = true)
{
	ItemCountData data{name};
	if (enumAllScenes && (s.GetType() != SceneSelection::Type::SCENE)) {
		auto enumScenes = [](void *param, obs_source_t *source) {
			if (!source) {
				return true;
			}
			auto data = reinterpret_cast<ItemCountData *>(param);
			auto scene = obs_scene_from_source(source);
			obs_scene_enum_items(scene, countSceneItem, data);
			return true;
		};
		obs_enum_scenes(enumScenes, &data);
	} else {
		auto source = obs_weak_source_get_source(s.GetScene(false));
		auto scene = obs_scene_from_source(source);
		obs_scene_enum_items(scene, countSceneItem, &data);
		obs_source_release(source);
	}
	return data.count;
}

std::vector<obs_scene_item *>
SceneItemSelection::GetSceneItems(SceneSelection &sceneSelection)
{
	std::vector<obs_scene_item *> ret;
	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	std::string name;
	if (_type == Type::VARIABLE) {
		auto var = _variable.lock();
		if (!var) {
			return ret;
		}
		name = var->Value();
	} else {
		name = GetWeakSourceName(_sceneItem);
	}
	int count = getCountOfSceneItemOccurance(sceneSelection, name, false);
	auto items = getSceneItemsWithName(scene, name);
	obs_source_release(s);

	if (_idxType == SceneItemSelection::IdxType::ALL ||
	    _idxType == SceneItemSelection::IdxType::ANY) {
		ret = items;
	} else {
		// Index order starts at the bottom and increases to the top
		// As this might be confusing reverse that order internally
		int idx = count - 1 - _idx;

		if (idx >= 0 && idx < (int)items.size()) {
			obs_sceneitem_addref(items[idx]);
			ret.emplace_back(items[idx]);
		}

		for (auto item : items) {
			obs_sceneitem_release(item);
		}
	}
	return ret;
}

std::string SceneItemSelection::ToString()
{
	if (_type == Type::VARIABLE) {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		return var->Name();
	}
	return GetWeakSourceName(_sceneItem);
}

static bool enumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	QStringList *names = reinterpret_cast<QStringList *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, enumSceneItem, ptr);
	}
	auto name = obs_source_get_name(obs_sceneitem_get_source(item));
	names->append(name);
	return true;
}

void populateSceneItemSelection(QComboBox *list)
{
	QStringList names;
	obs_scene_enum_items(nullptr, enumSceneItem, &names);
	names.removeDuplicates();
	names.sort();
	list->addItems(names);
	addSelectionEntry(list, obs_module_text("AdvSceneSwitcher.selectItem"));
	list->setCurrentIndex(0);
}

QStringList GetSceneItemsList(SceneSelection &s)
{
	QStringList names;
	if (s.GetType() != SceneSelection::Type::SCENE) {
		auto enumScenes = [](void *param, obs_source_t *source) {
			if (!source) {
				return true;
			}
			QStringList *names =
				reinterpret_cast<QStringList *>(param);
			auto scene = obs_scene_from_source(source);
			obs_scene_enum_items(scene, enumSceneItem, names);
			return true;
		};

		obs_enum_scenes(enumScenes, &names);
	} else {
		auto source = obs_weak_source_get_source(s.GetScene(false));
		auto scene = obs_scene_from_source(source);
		obs_scene_enum_items(scene, enumSceneItem, &names);
		obs_source_release(source);
	}

	names.removeDuplicates();
	names.sort();
	return names;
}

void SceneItemSelectionWidget::Reset()
{
	auto previousSel = _currentSelection;
	PopulateItemSelection();
	SetSceneItem(previousSel);
}

void SceneItemSelectionWidget::PopulateItemSelection()
{
	_sceneItems->clear();
	addSelectionEntry(_sceneItems,
			  obs_module_text("AdvSceneSwitcher.selectItem"));
	_sceneItems->insertSeparator(_sceneItems->count());

	const QStringList variables = GetVariablesNameList();
	addSelectionGroup(_sceneItems, variables);
	_variablesEndIdx = _sceneItems->count();

	const QStringList sceneItmes = GetSceneItemsList(_scene);
	addSelectionGroup(_sceneItems, sceneItmes, false);
	_itemsEndIdx = _sceneItems->count();
	_sceneItems->setCurrentIndex(0);
}

SceneItemSelectionWidget::SceneItemSelectionWidget(QWidget *parent,
						   bool showAll,
						   AllSelectionType type)
	: QWidget(parent), _hasAllEntry(showAll), _allType(type)
{
	_sceneItems = new QComboBox();
	_idx = new QComboBox();

	_sceneItems->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	_idx->setSizeAdjustPolicy(QComboBox::AdjustToContents);

	populateSceneItemSelection(_sceneItems);

	QWidget::connect(_sceneItems,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SelectionChanged(const QString &)));
	QWidget::connect(_idx, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(IdxChanged(int)));
	// Variables
	QWidget::connect(window(), SIGNAL(VariableAdded(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(window(), SIGNAL(VariableRemoved(const QString &)),
			 this, SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		window(),
		SIGNAL(VariableRenamed(const QString &, const QString &)), this,
		SLOT(ItemRename(const QString &, const QString &)));

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_idx);
	layout->addWidget(_sceneItems);
	setLayout(layout);
	_idx->hide();
}

void SceneItemSelectionWidget::SetSceneItem(const SceneItemSelection &item)
{
	int itemIdx = 0;
	switch (item._type) {
	case SceneItemSelection::Type::SOURCE: {
		int idx = item._idx;
		if (_hasAllEntry) {
			idx += 1;
		}
		_idx->setCurrentIndex(idx);
		itemIdx = findIdxInRagne(_sceneItems, _variablesEndIdx,
					 _itemsEndIdx,
					 GetWeakSourceName(item._sceneItem));
		_sceneItems->setCurrentIndex(itemIdx);
		break;
	}
	case SceneItemSelection::Type::VARIABLE: {

		auto var = item._variable.lock();
		if (!var) {
			break;
		}
		itemIdx = findIdxInRagne(_sceneItems, _selectIdx,
					 _variablesEndIdx, var->Name());
		_sceneItems->setCurrentIndex(itemIdx);
		break;
	}
	}

	switch (item._idxType) {
	case SceneItemSelection::IdxType::ALL:
	case SceneItemSelection::IdxType::ANY:
		_allType = AllSelectionType::ALL;
		_idx->setCurrentIndex(0);
		break;
	case SceneItemSelection::IdxType::INDIVIDUAL:
		int idx = item._idx;
		if (_hasAllEntry) {
			idx += 1;
		}
		_idx->setCurrentIndex(idx);
		break;
	}
	_currentSelection = item;
}

void SceneItemSelectionWidget::SetScene(const SceneSelection &s)
{
	_scene = s;
	_sceneItems->clear();
	_idx->hide();
	PopulateItemSelection();
}

void SceneItemSelectionWidget::SetShowAll(bool value)
{
	_hasAllEntry = value;
}

void SceneItemSelectionWidget::SetShowAllSelectionType(AllSelectionType t,
						       bool resetSelection)
{
	_allType = t;
	if (resetSelection) {
		_sceneItems->setCurrentIndex(0);
	} else {
		auto count = _idx->count() - 1;
		const QSignalBlocker b(_idx);
		SetupIdxSelection(count > 0 ? count : 1);
	}
}

void SceneItemSelectionWidget::SceneChanged(const SceneSelection &s)
{
	SetScene(s);
	adjustSize();
}

void SceneItemSelectionWidget::SelectionChanged(const QString &name)
{
	SceneItemSelection s;
	int sceneItemCount =
		getCountOfSceneItemOccurance(_scene, name.toStdString());
	if (sceneItemCount > 1) {
		_idx->show();
		SetupIdxSelection(sceneItemCount);
	} else {
		_idx->hide();
	}

	if (_hasAllEntry) {
		switch (_allType) {
		case SceneItemSelectionWidget::AllSelectionType::ALL:
			s._idxType = SceneItemSelection::IdxType::ALL;
			break;
		case SceneItemSelectionWidget::AllSelectionType::ANY:
			s._idxType = SceneItemSelection::IdxType::ANY;
			break;
		}
	}

	const int idx = _sceneItems->currentIndex();
	if (idx < _variablesEndIdx) {
		s._type = SceneItemSelection::Type::VARIABLE;
		s._variable = GetWeakVariableByQString(name);
	} else if (idx < _itemsEndIdx) {
		s._type = SceneItemSelection::Type::SOURCE;
		s._sceneItem = GetWeakSourceByQString(name);
	}

	_currentSelection = s;
	emit SceneItemChanged(s);
}

void SceneItemSelectionWidget::IdxChanged(int idx)
{
	if (idx < 0) {
		return;
	}

	_currentSelection._idx = idx;
	if (_hasAllEntry && idx == 0) {
		switch (_allType) {
		case SceneItemSelectionWidget::AllSelectionType::ALL:
			_currentSelection._idxType =
				SceneItemSelection::IdxType::ALL;
			break;
		case SceneItemSelectionWidget::AllSelectionType::ANY:
			_currentSelection._idxType =
				SceneItemSelection::IdxType::ANY;
			break;
		}
	}
	if (_hasAllEntry && idx > 0) {
		_currentSelection._idx -= 1;
		_currentSelection._idxType =
			SceneItemSelection::IdxType::INDIVIDUAL;
	}
	emit SceneItemChanged(_currentSelection);
}

void SceneItemSelectionWidget::ItemAdd(const QString &)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}

void SceneItemSelectionWidget::ItemRemove(const QString &)
{
	Reset();
}

void SceneItemSelectionWidget::ItemRename(const QString &, const QString &)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}

void SceneItemSelectionWidget::SetupIdxSelection(int sceneItemCount)
{
	_idx->clear();
	if (_hasAllEntry) {
		if (_allType == AllSelectionType::ALL) {
			_idx->addItem(obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.all"));
		} else {
			_idx->addItem(obs_module_text(
				"AdvSceneSwitcher.sceneItemSelection.any"));
		}
	}
	for (int i = 1; i <= sceneItemCount; ++i) {
		_idx->addItem(QString::number(i) + ".");
	}
	adjustSize();
}
