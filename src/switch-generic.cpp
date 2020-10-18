#include <QWidget>
#include <QHBoxLayout>

#include "headers/advanced-scene-switcher.hpp"

bool SceneSwitcherEntry::initialized()
{
	return (usePreviousScene || WeakSourceValid(scene)) && transition;
}

bool SceneSwitcherEntry::valid()
{
	return !initialized() ||
	       ((usePreviousScene || WeakSourceValid(scene)) &&
		WeakSourceValid(transition));
}

void SceneSwitcherEntry::logMatch()
{
	const char *sceneName = previous_scene_name;
	if (!usePreviousScene) {
		OBSSource s = obs_weak_source_get_source(scene);
		const char *sceneName = obs_source_get_name(s);
	}
	blog(LOG_INFO, "match for '%s' - switch to scene '%s'", getType(),
	     sceneName);
}

SwitchWidget::SwitchWidget(SceneSwitcherEntry *s, bool usePreviousScene)
{
	scenes = new QComboBox();
	transitions = new QComboBox();

	QWidget::connect(scenes, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(SceneChanged(const QString &)));
	QWidget::connect(transitions,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(TransitionChanged(const QString &)));

	AdvSceneSwitcher::populateSceneSelection(scenes, usePreviousScene);
	AdvSceneSwitcher::populateTransitionSelection(transitions);

	if (s) {
		if (s->usePreviousScene)
			scenes->setCurrentText(previous_scene_name);
		else
			scenes->setCurrentText(
				GetWeakSourceName(s->scene).c_str());
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

void SwitchWidget::SceneChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->usePreviousScene = text.compare(previous_scene_name) == 0;
	switchData->scene = GetWeakSourceByQString(text);
}

void SwitchWidget::TransitionChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->transition = GetWeakTransitionByQString(text);
}
