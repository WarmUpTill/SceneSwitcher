#include "headers/scene-selection.hpp"
#include "headers/advanced-scene-switcher.hpp"

void SceneSelection::Save(obs_data_t *obj, const char *name,
			  const char *typeName)
{
	obs_data_set_int(obj, typeName, static_cast<int>(_type));

	switch (_type) {
	case SceneSelectionType::SCENE:
		obs_data_set_string(obj, name,
				    GetWeakSourceName(_scene).c_str());
		break;
	case SceneSelectionType::GROUP:
		obs_data_set_string(obj, name, _group->name.c_str());
		break;
	default:
		break;
	}
}

void SceneSelection::Load(obs_data_t *obj, const char *name,
			  const char *typeName)
{
	_type = static_cast<SceneSelectionType>(
		obs_data_get_int(obj, typeName));
	auto target = obs_data_get_string(obj, name);
	switch (_type) {
	case SceneSelectionType::SCENE:
		_scene = GetWeakSourceByName(target);
		break;
	case SceneSelectionType::GROUP:
		_group = GetSceneGroupByName(target);
		break;
	case SceneSelectionType::PREVIOUS:
		break;
	case SceneSelectionType::CURRENT:
		break;
	default:
		break;
	}
}

OBSWeakSource SceneSelection::GetScene(bool advance)
{
	switch (_type) {
	case SceneSelectionType::SCENE:
		return _scene;
	case SceneSelectionType::GROUP:
		if (!_group) {
			return nullptr;
		}
		if (advance) {
			return _group->getNextScene();
		}
		return _group->getCurrentScene();
	case SceneSelectionType::PREVIOUS:
		return switcher->previousScene;
	case SceneSelectionType::CURRENT:
		return switcher->currentScene;
	default:
		break;
	}
	return nullptr;
}

std::string SceneSelection::ToString()
{
	switch (_type) {
	case SceneSelectionType::SCENE:
		return GetWeakSourceName(_scene);
	case SceneSelectionType::GROUP:
		if (_group) {
			return _group->name;
		}
		break;
	case SceneSelectionType::PREVIOUS:
		return obs_module_text("AdvSceneSwitcher.selectPreviousScene");
	case SceneSelectionType::CURRENT:
		return obs_module_text("AdvSceneSwitcher.selectCurrentScene");
	default:
		break;
	}
	return "";
}

SceneSelectionWidget::SceneSelectionWidget(QWidget *parent, bool sceneGroups,
					   bool previous, bool current)
	: QComboBox(parent)
{
	// For the rare occasion of a name conflict with current / previous
	setDuplicatesEnabled(true);
	populateSceneSelection(this, previous, current, false, sceneGroups,
			       &switcher->sceneGroups);

	QWidget::connect(this, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SelectionChanged(const QString &)));
	QWidget::connect(parent, SIGNAL(SceneGroupAdded(const QString &)), this,
			 SLOT(SceneGroupAdd(const QString &)));
	QWidget::connect(parent, SIGNAL(SceneGroupRemoved(const QString &)),
			 this, SLOT(SceneGroupRemove(const QString &)));
	QWidget::connect(
		parent,
		SIGNAL(SceneGroupRenamed(const QString &, const QString &)),
		this, SLOT(SceneGroupRename(const QString &, const QString &)));
}

void SceneSelectionWidget::SetScene(SceneSelection &s)
{
	// Order of entries
	// 1. Any Scene (current not used)
	// 2. Current Scene
	// 3. Previous Scene
	// 4. Scenes / Scene Groups

	int idx;

	switch (s.GetType()) {
	case SceneSelectionType::SCENE:
	case SceneSelectionType::GROUP:
		setCurrentText(QString::fromStdString(s.ToString()));
		break;
	case SceneSelectionType::PREVIOUS:
		idx = findText(QString::fromStdString(obs_module_text(
			"AdvSceneSwitcher.selectPreviousScene")));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	case SceneSelectionType::CURRENT:
		idx = findText(QString::fromStdString(obs_module_text(
			"AdvSceneSwitcher.selectCurrentScene")));
		if (idx != -1) {
			setCurrentIndex(idx);
		}
		break;
	default:
		setCurrentIndex(0);
		break;
	}
}

static int isFirstEntry(QComboBox *l, QString name, int idx)
{
	for (auto i = l->count() - 1; i >= 0; i--) {
		if (l->itemText(i) == name) {
			return idx == i;
		}
	}

	// If entry cannot be found we dont want the selection to be empty
	return false;
}

bool SceneSelectionWidget::IsCurrentSceneSelected(const QString &name)
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.selectCurrentScene")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

bool SceneSelectionWidget::IsPreviousSceneSelected(const QString &name)
{
	if (name == QString::fromStdString((obs_module_text(
			    "AdvSceneSwitcher.selectPreviousScene")))) {
		return isFirstEntry(this, name, currentIndex());
	}
	return false;
}

void SceneSelectionWidget::SelectionChanged(const QString &name)
{
	SceneSelection s;
	auto scene = GetWeakSourceByQString(name);
	if (scene) {
		s._type = SceneSelectionType::SCENE;
		s._scene = scene;
	}

	auto group = GetSceneGroupByQString(name);
	if (group) {
		s._type = SceneSelectionType::GROUP;
		s._scene = nullptr;
		s._group = group;
	}

	if (!scene && !group) {
		if (IsCurrentSceneSelected(name)) {
			s._type = SceneSelectionType::CURRENT;
		}
		if (IsPreviousSceneSelected(name)) {
			s._type = SceneSelectionType::PREVIOUS;
		}
	}

	emit SceneChanged(s);
}

void SceneSelectionWidget::SceneGroupAdd(const QString &name)
{
	addItem(name);
}

void SceneSelectionWidget::SceneGroupRemove(const QString &name)
{
	int idx = findText(name);

	if (idx == -1) {
		return;
	}

	int curIdx = currentIndex();
	removeItem(idx);

	if (curIdx == idx) {
		SceneSelection s;
		emit SceneChanged(s);
	}

	setCurrentIndex(0);
}

static int findLastOf(QComboBox *l, QString name)
{
	int idx = 0;
	for (auto i = l->count() - 1; i >= 0; i--) {
		if (l->itemText(i) == name) {
			return idx;
		}
	}
	return idx;
}

void SceneSelectionWidget::SceneGroupRename(const QString &oldName,
					    const QString &newName)
{
	bool renameSelected = currentText() == oldName;
	int idx = findText(oldName);

	if (idx == -1) {
		return;
	}

	removeItem(idx);
	insertItem(idx, newName);

	if (renameSelected) {
		setCurrentIndex(findLastOf(this, newName));
	}
}
