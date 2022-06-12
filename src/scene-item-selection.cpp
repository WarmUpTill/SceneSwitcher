#include "headers/scene-item-selection.hpp"

#include <obs-module.h>

void SceneItemSelection::Save(obs_data_t *obj, const char *name,
			      const char *targetName, const char *idxName)
{
	obs_data_set_int(obj, targetName, static_cast<int>(_target));
	if (_target == SceneItemSelection::Target::INDIVIDUAL) {
		obs_data_set_int(obj, idxName, _idx);
	} else {
		obs_data_set_int(obj, idxName, 0);
	}
	obs_data_set_string(obj, name, GetWeakSourceName(_sceneItem).c_str());
}

void SceneItemSelection::Load(obs_data_t *obj, const char *name,
			      const char *targetName, const char *idxName)
{
	_target = static_cast<SceneItemSelection::Target>(
		obs_data_get_int(obj, targetName));
	_idx = obs_data_get_int(obj, idxName);
	auto sceneItemName = obs_data_get_string(obj, name);
	_sceneItem = GetWeakSourceByName(sceneItemName);
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

int getCountOfSceneItemOccurance(SceneSelection &s, std::string &name,
				 bool enumAllScenes = true)
{
	ItemCountData data{name};
	if (enumAllScenes && (s.GetType() == SceneSelectionType::CURRENT ||
			      s.GetType() == SceneSelectionType::PREVIOUS)) {
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
	auto s = obs_weak_source_get_source(sceneSelection.GetScene(false));
	auto scene = obs_scene_from_source(s);
	auto name = GetWeakSourceName(_sceneItem);
	int count = getCountOfSceneItemOccurance(sceneSelection, name, false);
	auto items = getSceneItemsWithName(scene, name);
	obs_source_release(s);

	std::vector<obs_scene_item *> ret;

	if (_target == SceneItemSelection::Target::ALL ||
	    _target == SceneItemSelection::Target::ANY) {
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
	return GetWeakSourceName(_sceneItem);
}

SceneItemSelectionWidget::SceneItemSelectionWidget(QWidget *parent,
						   bool showAll,
						   AllSelectionType type)
	: QWidget(parent), _showAll(showAll), _allType(type)
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
	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_idx);
	layout->addWidget(_sceneItems);
	setLayout(layout);
	_idx->hide();
}

void SceneItemSelectionWidget::SetSceneItem(const SceneItemSelection &item)
{
	_sceneItems->setCurrentText(
		QString::fromStdString(GetWeakSourceName(item._sceneItem)));
	if (item._target == SceneItemSelection::Target::ALL) {
		_allType = AllSelectionType::ALL;
		_idx->setCurrentIndex(0);
	} else if (item._target == SceneItemSelection::Target::ANY) {
		_allType = AllSelectionType::ANY;
		_idx->setCurrentIndex(0);
	} else {
		int idx = item._idx;
		if (_showAll) {
			idx += 1;
		}
		_idx->setCurrentIndex(idx);
	}
}

void SceneItemSelectionWidget::SetScene(const SceneSelection &s)
{
	_scene = s;
	_sceneItems->clear();
	_idx->hide();
	populateSceneItemSelection(_sceneItems, _scene);
}

void SceneItemSelectionWidget::SetShowAll(bool value)
{
	_showAll = value;
}

void SceneItemSelectionWidget::SetShowAllSelectionType(AllSelectionType t)
{
	_allType = t;
	_sceneItems->setCurrentIndex(0);
}

void SceneItemSelectionWidget::SceneChanged(const SceneSelection &s)
{
	SetScene(s);
	adjustSize();
}

void SceneItemSelectionWidget::SelectionChanged(const QString &name)
{
	SceneItemSelection s;
	_sceneItem = GetWeakSourceByQString(name);
	s._sceneItem = _sceneItem;
	if (_allType == AllSelectionType::ALL) {
		s._target = SceneItemSelection::Target::ALL;
	} else {
		s._target = SceneItemSelection::Target::ANY;
	}
	auto stdName = name.toStdString();
	int sceneItemCount = getCountOfSceneItemOccurance(_scene, stdName);
	if (sceneItemCount > 1) {
		_idx->show();
		SetupIdxSelection(sceneItemCount);
	} else {
		_idx->hide();
	}
	emit SceneItemChanged(s);
}

void SceneItemSelectionWidget::IdxChanged(int idx)
{
	if (idx < 0) {
		return;
	}

	SceneItemSelection s;
	s._sceneItem = _sceneItem;
	if (_showAll && idx == 0) {
		if (_allType == AllSelectionType::ALL) {
			s._target = SceneItemSelection::Target::ALL;
		} else {
			s._target = SceneItemSelection::Target::ANY;
		}
		s._idx = 0;
	} else {
		s._target = SceneItemSelection::Target::INDIVIDUAL;
		if (_showAll) {
			idx -= 1;
		}
		s._idx = idx;
	}
	emit SceneItemChanged(s);
}

void SceneItemSelectionWidget::SetupIdxSelection(int sceneItemCount)
{
	_idx->clear();
	if (_showAll) {
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
