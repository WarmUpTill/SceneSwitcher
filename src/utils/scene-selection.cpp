#include "scene-selection.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

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
	case Type::VARIABLE:
		_variable = GetWeakVariableByName(targetName);
		break;
	default:
		break;
	}
	obs_data_release(data);
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
		return switcher->previousScene;
	case Type::CURRENT:
		return switcher->currentScene;
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return nullptr;
		}
		return GetWeakSourceByName(var->Value().c_str());
	}
	default:
		break;
	}
	return nullptr;
}

std::string SceneSelection::ToString() const
{
	switch (_type) {
	case Type::SCENE:
		return GetWeakSourceName(_scene);
	case Type::GROUP:
		if (_group) {
			return _group->name;
		}
		break;
	case Type::PREVIOUS:
		return obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	case Type::CURRENT:
		return obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	case Type::VARIABLE: {
		auto var = _variable.lock();
		if (!var) {
			return "";
		}
		return var->Name();
	}
	default:
		break;
	}
	return "";
}

SceneSelection SceneSelectionWidget::CurrentSelection()
{
	SceneSelection s;
	const int idx = currentIndex();
	const auto name = currentText();

	if (idx < _orderEndIdx) {
		if (IsCurrentSceneSelected(name)) {
			s._type = SceneSelection::Type::CURRENT;
		}
		if (IsPreviousSceneSelected(name)) {
			s._type = SceneSelection::Type::PREVIOUS;
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

static QStringList getOrderList(bool current, bool previous)
{
	QStringList list;
	if (current) {
		list << obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	}
	if (previous) {
		list << obs_module_text("AdvSceneSwitcher.selectPreviousScene");
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
	for (const auto &sg : switcher->sceneGroups) {
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
	addSelectionEntry(this,
			  obs_module_text("AdvSceneSwitcher.selectScene"));
	insertSeparator(count());

	if (_current || _previous) {
		const QStringList order = getOrderList(_current, _previous);
		addSelectionGroup(this, order);
	}
	_orderEndIdx = count();

	if (_variables) {
		const QStringList variables = GetVariablesNameList();
		addSelectionGroup(this, variables);
	}
	_variablesEndIdx = count();

	if (_sceneGroups) {
		const QStringList sceneGroups = getSceneGroupsList();
		addSelectionGroup(this, sceneGroups);
	}
	_groupsEndIdx = count();

	const QStringList scenes = getScenesList();
	addSelectionGroup(this, scenes);
	_scenesEndIdx = count();

	// Remove last separator
	removeItem(count() - 1);
	setCurrentIndex(0);
}

SceneSelectionWidget::SceneSelectionWidget(QWidget *parent, bool variables,
					   bool sceneGroups, bool previous,
					   bool current)
	: QComboBox(parent),
	  _current(current),
	  _previous(previous),
	  _variables(variables),
	  _sceneGroups(sceneGroups)
{
	setDuplicatesEnabled(true);
	PopulateSelection();

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));

	// Scene groups
	QWidget::connect(window(), SIGNAL(SceneGroupAdded(const QString &)),
			 this, SLOT(ItemAdd(const QString &)));
	QWidget::connect(window(), SIGNAL(SceneGroupRemoved(const QString &)),
			 this, SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		window(),
		SIGNAL(SceneGroupRenamed(const QString &, const QString &)),
		this, SLOT(ItemRename(const QString &, const QString &)));

	// Variables
	QWidget::connect(window(), SIGNAL(VariableAdded(const QString &)), this,
			 SLOT(ItemAdd(const QString &)));
	QWidget::connect(window(), SIGNAL(VariableRemoved(const QString &)),
			 this, SLOT(ItemRemove(const QString &)));
	QWidget::connect(
		window(),
		SIGNAL(VariableRenamed(const QString &, const QString &)), this,
		SLOT(ItemRename(const QString &, const QString &)));
}

void SceneSelectionWidget::SetScene(const SceneSelection &s)
{
	int idx = 0;

	switch (s.GetType()) {
	case SceneSelection::Type::SCENE: {
		if (_scenesEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = findIdxInRagne(this, _groupsEndIdx, _scenesEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::GROUP: {
		if (_groupsEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = findIdxInRagne(this, _variablesEndIdx, _groupsEndIdx,
				     s.ToString());
		break;
	}
	case SceneSelection::Type::PREVIOUS: {
		if (_orderEndIdx == -1) {
			idx = 0;
			break;
		}

		idx = findIdxInRagne(
			this, _selectIdx, _orderEndIdx,
			obs_module_text(
				"AdvSceneSwitcher.selectPreviousScene"));
		break;
	}
	case SceneSelection::Type::CURRENT: {
		if (_orderEndIdx == -1) {
			idx = 0;
			break;
		}

		idx = findIdxInRagne(
			this, _selectIdx, _orderEndIdx,
			obs_module_text("AdvSceneSwitcher.selectCurrentScene"));
		break;
	}
	case SceneSelection::Type::VARIABLE: {
		if (_variablesEndIdx == -1) {
			idx = 0;
			break;
		}
		idx = findIdxInRagne(this, _orderEndIdx, _variablesEndIdx,
				     s.ToString());
		break;
	default:
		idx = 0;
		break;
	}
	}
	setCurrentIndex(idx);
	_currentSelection = s;
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

void SceneSelectionWidget::SelectionChanged(const QString &name)
{
	_currentSelection = CurrentSelection();
	emit SceneChanged(_currentSelection);
}

void SceneSelectionWidget::ItemAdd(const QString &name)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}

bool SceneSelectionWidget::NameUsed(const QString &name)
{
	if (_currentSelection._type == SceneSelection::Type::GROUP &&
	    _currentSelection._group &&
	    _currentSelection._group->name == name.toStdString()) {
		return true;
	}
	if (_currentSelection._type == SceneSelection::Type::VARIABLE) {
		auto var = _currentSelection._variable.lock();
		if (var && var->Name() == name.toStdString()) {
			return true;
		}
	}
	return false;
}

void SceneSelectionWidget::ItemRemove(const QString &name)
{
	if (!NameUsed(name)) {
		blockSignals(true);
	}
	Reset();
	blockSignals(false);
}

void SceneSelectionWidget::ItemRename(const QString &oldName,
				      const QString &newName)
{
	blockSignals(true);
	Reset();
	blockSignals(false);
}
