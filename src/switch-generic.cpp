#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool SceneSwitcherEntry::initialized()
{
	return (usePreviousScene || WeakSourceValid(scene) ||
		SceneGroupValid(group)) &&
	       transition;
}

bool SceneSwitcherEntry::valid()
{
	return !initialized() || ((usePreviousScene || WeakSourceValid(scene) ||
				   SceneGroupValid(group)) &&
				  WeakSourceValid(transition));
}

void SceneSwitcherEntry::logMatchScene()
{
	std::string sceneName = previous_scene_name;
	if (!usePreviousScene) {
		sceneName = GetWeakSourceName(scene);
	}
	blog(LOG_INFO, "match for '%s' - switch to scene '%s'", getType(),
	     sceneName.c_str());
}

void SceneSwitcherEntry::logMatchSceneGroup()
{
	if (group->scenes.empty()) {
		blog(LOG_INFO,
		     "match for '%s' - but no scenes specified in '%s'",
		     getType(), group->name.c_str());
		return;
	}

	std::string sceneName = GetWeakSourceName(group->getCurrentScene());

	blog(LOG_INFO, "match for '%s' - switch to scene '%s' using '%s'",
	     getType(), sceneName.c_str(), group->name.c_str());
}

void SceneSwitcherEntry::logMatch()
{
	if (targetType == SwitchTargetType::Scene) {
		logMatchScene();
	} else if (targetType == SwitchTargetType::SceneGroup) {
		logMatchSceneGroup();
	}
}

OBSWeakSource SceneSwitcherEntry::getScene()
{
	if (targetType == SwitchTargetType::Scene) {
		if (usePreviousScene && switcher)
			return switcher->previousScene;
		return scene;
	} else if (targetType == SwitchTargetType::SceneGroup) {
		return group->getNextScene();
	}
	return nullptr;
}

void SceneSwitcherEntry::save(obs_data_t *obj, const char *targetTypeSaveName,
			      const char *targetSaveName,
			      const char *transitionSaveName)
{
	obs_data_set_int(obj, targetTypeSaveName, static_cast<int>(targetType));

	std::string targetName = "";

	if (targetType == SwitchTargetType::Scene) {
		if (usePreviousScene) {
			targetName = previous_scene_name;
		} else {
			targetName = GetWeakSourceName(scene);
		}
	} else if (targetType == SwitchTargetType::SceneGroup) {
		targetName = group->name.c_str();
	}

	obs_data_set_string(obj, targetSaveName, targetName.c_str());

	obs_data_set_string(obj, transitionSaveName,
			    GetWeakSourceName(transition).c_str());
}

void SceneSwitcherEntry::load(obs_data_t *obj, const char *targetTypeLoadName,
			      const char *targetLoadName,
			      const char *transitionLoadName)
{
	targetType = static_cast<SwitchTargetType>(
		obs_data_get_int(obj, targetTypeLoadName));

	const char *targetName = obs_data_get_string(obj, targetLoadName);

	if (targetType == SwitchTargetType::Scene) {
		usePreviousScene = strcmp(targetName, previous_scene_name) == 0;
		if (!usePreviousScene)
			scene = GetWeakSourceByName(targetName);
	} else if (targetType == SwitchTargetType::SceneGroup) {
		group = GetSceneGroupByName(targetName);
	}

	const char *transitionName =
		obs_data_get_string(obj, transitionLoadName);
	transition = GetWeakTransitionByName(transitionName);

	usePreviousScene = strcmp(targetName, previous_scene_name) == 0;
}

void SwitchWidget::SceneGroupAdd(const QString &name)
{
	if (!scenes)
		return;

	scenes->addItem(name);
}

void SwitchWidget::SceneGroupRemove(const QString &name)
{
	if (!scenes)
		return;

	int idx = scenes->findText(name);

	if (idx == -1) {
		return;
	}

	scenes->removeItem(idx);

	if (switchData && switchData->group == GetSceneGroupByQString(name)) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switchData->targetType = SwitchTargetType::Scene;
		switchData->scene = nullptr;
	}

	scenes->setCurrentIndex(0);
}

void SwitchWidget::SceneGroupRename(const QString &oldName,
				    const QString &newName)
{
	if (!scenes)
		return;

	bool renameSelected = scenes->currentText() == oldName;
	int idx = scenes->findText(oldName);

	if (idx == -1) {
		return;
	}

	scenes->removeItem(idx);
	scenes->insertItem(idx, newName);

	if (renameSelected)
		scenes->setCurrentIndex(scenes->findText(newName));
}

SwitchWidget::SwitchWidget(QWidget *parent, SceneSwitcherEntry *s,
			   bool usePreviousScene, bool addSceneGroup)
{
	scenes = new QComboBox();
	transitions = new QComboBox();

	// Depending on selected OBS theme some widgets might have a different
	// background color than the listwidget and might look out of place
	setStyleSheet("QLabel { background-color: transparent; }\
		       QSlider { background-color: transparent; }\
		       QCheckBox { background-color: transparent; }");

	QWidget::connect(scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));
	QWidget::connect(transitions,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(TransitionChanged(const QString &)));

	QWidget::connect(parent, SIGNAL(SceneGroupAdded(const QString &)), this,
			 SLOT(SceneGroupAdd(const QString &)));
	QWidget::connect(parent, SIGNAL(SceneGroupRemoved(const QString &)),
			 this, SLOT(SceneGroupRemove(const QString &)));
	QWidget::connect(
		parent,
		SIGNAL(SceneGroupRenamed(const QString &, const QString &)),
		this, SLOT(SceneGroupRename(const QString &, const QString &)));

	AdvSceneSwitcher::populateSceneSelection(scenes, usePreviousScene,
						 addSceneGroup);
	AdvSceneSwitcher::populateTransitionSelection(transitions);

	switchData = s;
	showSwitchData();
}

SceneSwitcherEntry *SwitchWidget::getSwitchData()
{
	return switchData;
}

void SwitchWidget::setSwitchData(SceneSwitcherEntry *s)
{
	switchData = s;
}

void SwitchWidget::showSwitchData()
{
	if (!switchData)
		return;

	transitions->setCurrentText(
		GetWeakSourceName(switchData->transition).c_str());

	if (switchData->usePreviousScene) {
		scenes->setCurrentText(obs_module_text(
			"AdvSceneSwitcher.selectPreviousScene"));
		return;
	}

	scenes->setCurrentText(GetWeakSourceName(switchData->scene).c_str());

	if (switchData->group &&
	    switchData->targetType == SwitchTargetType::SceneGroup) {
		scenes->setCurrentText(
			QString::fromStdString(switchData->group->name));
	}
}

void SwitchWidget::swapSwitchData(SwitchWidget *s1, SwitchWidget *s2)
{
	SceneSwitcherEntry *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

bool isPreviousScene(const QString &text)
{
	return text.compare(obs_module_text(
		       "AdvSceneSwitcher.selectPreviousScene")) == 0;
}

void SwitchWidget::SceneChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);

	switchData->usePreviousScene = isPreviousScene(text);
	if (switchData->usePreviousScene) {
		switchData->targetType = SwitchTargetType::Scene;
		return;
	}

	switchData->scene = GetWeakSourceByQString(text);
	switchData->targetType = SwitchTargetType::Scene;

	if (!switchData->scene) {
		switchData->group = GetSceneGroupByQString(text);
		switchData->targetType = SwitchTargetType::SceneGroup;
	}
}

void SwitchWidget::TransitionChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->transition = GetWeakTransitionByQString(text);
}
