#include "scene-selection.hpp"
#include "obs-module-helper.hpp"
#include "scene-group.hpp"
#include "scene-switch-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "variable.hpp"

#include <obs-frontend-api.h>

namespace advss {

constexpr std::string_view typeSaveName = "type";
constexpr std::string_view nameSaveName = "name";
constexpr std::string_view selectionSaveName = "sceneSelection";

void SceneSelection::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, typeSaveName.data(), static_cast<int>(_type));
	switch (_type) {
	case Type::SCENE:
		obs_data_set_string(data, nameSaveName.data(),
				    GetWeakSourceName(_scene).c_str());
		break;
	case Type::GROUP:
		obs_data_set_string(data, nameSaveName.data(),
				    _group->name.c_str());
		break;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			break;
		}
		obs_data_set_string(data, nameSaveName.data(),
				    var->Name().c_str());
		break;
	}
	default:
		break;
	}
	obs_data_set_obj(obj, selectionSaveName.data(), data);
	obs_data_release(data);
}

void SceneSelection::Load(obs_data_t *obj, const char *name,
			  const char *typeName)
{
	// TODO: Remove in future version
	if (!obs_data_has_user_value(obj, selectionSaveName.data())) {
		_type = static_cast<Type>(obs_data_get_int(obj, typeName));
		auto targetName = obs_data_get_string(obj, name);
		switch (_type) {
		case Type::SCENE:
			_scene = GetWeakSourceByName(targetName);
			break;
		case Type::GROUP:
			_group = GetSceneGroupByName(targetName);
			break;
		case Type::PREVIOUS:
			break;
		case Type::CURRENT:
			break;
		default:
			break;
		}
		return;
	}

	auto data = obs_data_get_obj(obj, selectionSaveName.data());
	_type = static_cast<Type>(obs_data_get_int(data, typeSaveName.data()));
	auto targetName = obs_data_get_string(data, nameSaveName.data());
	switch (_type) {
	case Type::SCENE:
		_scene = GetWeakSourceByName(targetName);
		break;
	case Type::GROUP:
		_group = GetSceneGroupByName(targetName);
		break;
	case Type::PREVIOUS:
		break;
	case Type::CURRENT:
		break;
	case Type::PREVIEW:
		break;
	case Type::VARIABLE:
		_variable = GetWeakVariableByName(targetName);
		break;
	default:
		break;
	}
	obs_data_release(data);
}

static bool IsScene(const OBSWeakSource &source)
{
	auto s = obs_weak_source_get_source(source);
	bool ret = !!obs_scene_from_source(s);
	obs_source_release(s);
	return ret;
}

OBSWeakSource SceneSelection::GetScene(bool advance) const
{
	switch (_type) {
	case Type::SCENE:
		return _scene;
	case Type::GROUP:
		if (!_group) {
			return nullptr;
		}
		if (advance) {
			return _group->getNextScene();
		}
		return _group->getCurrentScene();
	case Type::PREVIOUS:
		return GetPreviousScene();
	case Type::CURRENT:
		return GetCurrentScene();
	case Type::PREVIEW: {
		auto s = obs_frontend_get_current_preview_scene();
		auto scene = obs_source_get_weak_source(s);
		obs_weak_source_release(scene);
		obs_source_release(s);
		return scene;
	}
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return nullptr;
		}
		auto scene = GetWeakSourceByName(var->Value().c_str());
		if (IsScene(scene)) {
			return scene;
		}
		return nullptr;
	}
	default:
		break;
	}
	return nullptr;
}

std::string SceneSelection::ToString(bool resolve) const
{
	switch (_type) {
	case Type::SCENE:
		return GetWeakSourceName(_scene);
	case Type::GROUP:
		if (_group) {
			if (resolve) {
				return _group->name + "[" +
				       GetWeakSourceName(
					       _group->getCurrentScene()) +
				       "]";
			}
			return _group->name;
		}
		break;
	case Type::PREVIOUS:
		return obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	case Type::CURRENT:
		return obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	case Type::PREVIEW:
		return obs_module_text("AdvSceneSwitcher.selectPreviewScene");
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		if (resolve) {
			return var->Name() + "[" + var->Value() + "]";
		}
		return var->Name();
	}
	default:
		break;
	}
	return "";
}

void SceneSelection::ResolveVariables()
{
	_scene = GetScene();
	_type = Type::SCENE;
}

SceneSelection SceneSelectionWidget::CurrentSelection()
{
	SceneSelection s;
	const int idx = currentIndex();
	const auto name = currentText();
	if (idx == -1 || name.isEmpty()) {
		return s;
	}

	if (idx < _placeholderEndIdx) {
		if (IsCurrentSceneSelected(name)) {
			s._type = SceneSelection::Type::CURRENT;
		}
		if (IsPreviousSceneSelected(name)) {
			s._type = SceneSelection::Type::PREVIOUS;
		}
		if (IsPreviewSceneSelected(name)) {
			s._type = SceneSelection::Type::PREVIEW;
		}
	} else if (idx < _variablesEndIdx) {
		s._type = SceneSelection::Type::VARIABLE;
		s._variable = GetWeakVariableByQString(name);
	} else if (idx < _groupsEndIdx) {
		s._type = SceneSelection::Type::GROUP;
		s._group = GetSceneGroupByQString(name);
	} else if (idx < _scenesEndIdx) {
		s._type = SceneSelection::Type::SCENE;
		s._scene = GetWeakSourceByQString(name);
	}
	return s;
}

static QStringList getOrderList(bool current, bool previous, bool preview)
{
	QStringList list;
	if (current) {
		list << obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	}
	if (previous) {
		list << obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	}
	if (preview) {
		list << obs_module_text("AdvSceneSwitcher.selectPreviewScene");
	}
	return list;
}

static QStringList getScenesList()
{
	QStringList list;
	char **scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		list << name;
		temp++;
	}
	bfree(scenes);
	return list;
}

static QStringList getSceneGroupsList()
{
	QStringList list;
	for (const auto &sg : GetSceneGroups()) {
		list << QString::fromStdString(sg.name);
	}
	list.sort();
	return list;
}

void SceneSelectionWidget::Reset()
{
	auto previousSel = _currentSelection;
	PopulateSelection();
	SetScene(previousSel);
}

void SceneSelectionWidget::PopulateSelection()
{
	clear();
	if (_current || _previous) {
		const QStringList order =
			getOrderList(_current, _previous, _preview);
		AddSelectionGroup(this, order);
	}
	_placeholderEndIdx = count();

	if (_variables) {
		const QStringList variables = GetVariablesNameList();
		AddSelectionGroup(this, variables);
	}
	_variablesEndIdx = count();

	if (_sceneGroups) {
		const QStringList sceneGroups = getSceneGroupsList();
		AddSelectionGroup(this, sceneGroups);
	}
	_groupsEndIdx = count();

	const QStringList scenes = getScenesList();
	AddSelectionGroup(this, scenes);
	_scenesEndIdx = count();

	// Remove last separator
	removeItem(count() - 1);
	setCurrentIndex(-1);
}

SceneSelectionWidget::SceneSelectionWidget(QWidget *parent, bool variables,
					   bool sceneGroups, bool previous,
					   bool current, bool preview)
	: FilterComboBox(parent,
			 obs_module_text("AdvSceneSwitcher.selectScene")),
	  _current(current),
	  _previous(previous),
	  _preview(preview),
	  _variables(variables),
	  _sceneGroups(sceneGroups)
{
	setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(this, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SelectionChanged(int)));

	// Scene groups
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupAdded(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(GetSettingsWindow(),
			 SIGNAL(SceneGroupRemoved(const QString &)), this,
			 SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		GetSettingsWindow(),
		SIGNAL(SceneGroupRenamed(const QString &, const QString &)),
		this, SLOT(ItemRename(const QString &, const QString &)));

	// Variables
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Add(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Remove(const QString &)), this,
			 SLOT(ItemRemove(const QString &)));
	QWidget::connect(VariableSignalManager::Instance(),
			 SIGNAL(Rename(const QString &, const QString &)), this,
			 SLOT(ItemRename(const QString &, const QString &)));
}

void SceneSelectionWidget::SetScene(const SceneSelection &s)
{
	int idx = -1;

	switch (s.GetType()) {
	case SceneSelection::Type::SCENE: {
		if (_scenesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(this, _groupsEndIdx, _scenesEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::GROUP: {
		if (_groupsEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(this, _variablesEndIdx, _groupsEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::PREVIOUS: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			this, _selectIdx, _placeholderEndIdx,
			obs_module_text(
				"AdvSceneSwitcher.selectPreviousScene"));
		break;
	}
	case SceneSelection::Type::CURRENT: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			this, _selectIdx, _placeholderEndIdx,
			obs_module_text("AdvSceneSwitcher.selectCurrentScene"));
		break;
	}
	case SceneSelection::Type::PREVIEW: {
		if (_placeholderEndIdx == -1) {
			idx = -1;
			break;
		}

		idx = FindIdxInRagne(
			this, _selectIdx, _placeholderEndIdx,
			obs_module_text("AdvSceneSwitcher.selectPreviewScene"));
		break;
	}
	case SceneSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = -1;
			break;
		}
		idx = FindIdxInRagne(this, _placeholderEndIdx, _variablesEndIdx,
				     s.ToString());
		break;
	default:
		idx = -1;
		break;
	}
	}
	setCurrentIndex(idx);
	_currentSelection = s;
}

void SceneSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const QSignalBlocker b(this);
	Reset();
}

bool SceneSelectionWidget::IsCurrentSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectCurrentScene")));
}

bool SceneSelectionWidget::IsPreviousSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectPreviousScene")));
}

bool SceneSelectionWidget::IsPreviewSceneSelected(const QString &name)
{
	return name == QString::fromStdString((obs_module_text(
			       "AdvSceneSwitcher.selectPreviewScene")));
}

void SceneSelectionWidget::SelectionChanged(int)
{
	_currentSelection = CurrentSelection();
	emit SceneChanged(_currentSelection);
}

void SceneSelectionWidget::ItemAdd(const QString &)
{
	const QSignalBlocker b(this);
	Reset();
}

bool SceneSelectionWidget::NameUsed(const QString &name)
{
	if (_currentSelection._type == SceneSelection::Type::GROUP &&
	    _currentSelection._group &&
	    _currentSelection._group->name == name.toStdString()) {
		return true;
	}
	return _currentSelection._type == SceneSelection::Type::VARIABLE &&
	       currentText() == name;
}

void SceneSelectionWidget::ItemRemove(const QString &name)
{
	if (!NameUsed(name)) {
		blockSignals(true);
	}
	Reset();
	blockSignals(false);
}

void SceneSelectionWidget::ItemRename(const QString &, const QString &)
{
	const QSignalBlocker b(this);
	Reset();
}

} // namespace advss
