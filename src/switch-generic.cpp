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

void SceneSwitcherEntry::logMatch()
{
	const char *sceneName = previous_scene_name;
	if (!usePreviousScene) {
		obs_source_t *s = obs_weak_source_get_source(scene);
		sceneName = obs_source_get_name(s);
		obs_source_release(s);
	}
	blog(LOG_INFO, "match for '%s' - switch to scene '%s'", getType(),
	     sceneName);
}

OBSWeakSource SceneSwitcherEntry::getScene()
{
	OBSWeakSource nextScene = nullptr;
	if (targetType == SwitchTargetType::Scene) {
		nextScene = scene;

	} else {
		nextScene = group->getNextScene();
	}
	return nextScene;
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

	if (s) {
		if (s->usePreviousScene) {
			scenes->setCurrentText(obs_module_text(
				"AdvSceneSwitcher.selectPreviousScene"));
		} else {
			scenes->setCurrentText(
				GetWeakSourceName(s->scene).c_str());
			if (s->targetType == SwitchTargetType::SceneGroup &&
			    s->group)
				scenes->setCurrentText(
					QString::fromStdString(s->group->name));
		}
		transitions->setCurrentText(
			GetWeakSourceName(s->transition).c_str());
	}

	switchData = s;
}

SceneSwitcherEntry *SwitchWidget::getSwitchData()
{
	return switchData;
}

void SwitchWidget::setSwitchData(SceneSwitcherEntry *s)
{
	switchData = s;
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
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->usePreviousScene = isPreviousScene(text);
	if (switchData->usePreviousScene)
		return;

	switchData->scene = GetWeakSourceByQString(text);
	switchData->targetType = SwitchTargetType::Scene;

	if (!switchData->scene) {
		switchData->group = GetSceneGroupByQString(text);
		switchData->targetType = SwitchTargetType::SceneGroup;
	}
}

void SwitchWidget::TransitionChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->transition = GetWeakTransitionByQString(text);
}
