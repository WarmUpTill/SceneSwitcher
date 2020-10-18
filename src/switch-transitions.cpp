#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_transitionsAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->sceneTransitions.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->sceneTransitions);
	ui->sceneTransitions->addItem(item);
	TransitionSwitchWidget *sw =
		new TransitionSwitchWidget(&switcher->sceneTransitions.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->sceneTransitions->setItemWidget(item, sw);
}

void SceneSwitcher::on_transitionsRemove_clicked()
{
	QListWidgetItem *item = ui->sceneTransitions->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->sceneTransitions->currentRow();
		auto &switches = switcher->sceneTransitions;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SceneSwitcher::on_transitionsUp_clicked()
{
	int index = ui->sceneTransitions->currentRow();
	if (!listMoveUp(ui->sceneTransitions))
		return;

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index - 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTransitions[index],
		  switcher->sceneTransitions[index - 1]);
}

void SceneSwitcher::on_transitionsDown_clicked()
{
	int index = ui->sceneTransitions->currentRow();

	if (!listMoveDown(ui->sceneTransitions))
		return;

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->sceneTransitions->itemWidget(
			ui->sceneTransitions->item(index + 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->sceneTransitions[index],
		  switcher->sceneTransitions[index + 1]);
}

void SceneSwitcher::on_defaultTransitionsAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->defaultSceneTransitions.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->defaultTransitions);
	ui->defaultTransitions->addItem(item);
	DefTransitionSwitchWidget *sw = new DefTransitionSwitchWidget(
		&switcher->defaultSceneTransitions.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->defaultTransitions->setItemWidget(item, sw);
}

void SceneSwitcher::on_defaultTransitionsRemove_clicked()
{
	QListWidgetItem *item = ui->defaultTransitions->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->defaultTransitions->currentRow();
		auto &switches = switcher->defaultSceneTransitions;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SceneSwitcher::on_defaultTransitionsUp_clicked()
{
	int index = ui->defaultTransitions->currentRow();
	if (!listMoveUp(ui->defaultTransitions))
		return;

	TransitionSwitchWidget *s1 =
		(TransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index));
	TransitionSwitchWidget *s2 =
		(TransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index - 1));
	TransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->defaultSceneTransitions[index],
		  switcher->defaultSceneTransitions[index - 1]);
}

void SceneSwitcher::on_defaultTransitionsDown_clicked()
{
	int index = ui->defaultTransitions->currentRow();

	if (!listMoveDown(ui->defaultTransitions))
		return;

	DefTransitionSwitchWidget *s1 =
		(DefTransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index));
	DefTransitionSwitchWidget *s2 =
		(DefTransitionSwitchWidget *)ui->defaultTransitions->itemWidget(
			ui->defaultTransitions->item(index + 1));
	DefTransitionSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->defaultSceneTransitions[index],
		  switcher->defaultSceneTransitions[index + 1]);
}

void SwitcherData::setDefaultSceneTransitions()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (DefaultSceneTransition &s : defaultSceneTransitions) {
		if (s.scene == ws) {
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			//This might cancel the current transition
			//There is no way to be sure when the previous transition finished
			obs_frontend_set_current_transition(transition);

			if (verbose)
				if (verbose)
					s.logMatch();

			obs_source_release(transition);
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SceneSwitcher::on_transitionOverridecheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		switcher->tansitionOverrideOverride = false;
	} else {
		switcher->tansitionOverrideOverride = true;
	}
}

obs_weak_source_t *getNextTransition(obs_weak_source_t *scene1,
				     obs_weak_source_t *scene2)
{
	obs_weak_source_t *ws = nullptr;
	if (scene1 && scene2) {
		for (SceneTransition &t : switcher->sceneTransitions) {
			if (t.scene == scene1 && t.scene2 == scene2)
				ws = t.transition;
		}
		obs_weak_source_addref(ws);
	}
	return ws;
}

void overwriteTransitionOverride(obs_weak_source_t *sceneWs,
				 obs_source_t *transition, transitionData &td)
{
	obs_source_t *scene = obs_weak_source_get_source(sceneWs);
	obs_data_t *data = obs_source_get_private_settings(scene);

	td.name = obs_data_get_string(data, "transition");
	td.duration = obs_data_get_int(data, "duration");

	const char *name = obs_source_get_name(transition);
	int duration = obs_frontend_get_transition_duration();

	obs_data_set_string(data, "transition", name);
	obs_data_set_int(data, "transition_duration", duration);

	obs_data_release(data);
	obs_source_release(scene);
}

void restoreTransitionOverride(obs_source_t *scene, transitionData td)
{
	obs_data_t *data = obs_source_get_private_settings(scene);

	obs_data_set_string(data, "transition", td.name.c_str());
	obs_data_set_int(data, "transition_duration", td.duration);

	obs_data_release(data);
}

void setNextTransition(OBSWeakSource &targetScene, obs_source_t *currentSource,
		       OBSWeakSource &transition,
		       bool &transitionOverrideOverride, transitionData &td)
{
	obs_weak_source_t *currentScene =
		obs_source_get_weak_source(currentSource);
	obs_weak_source_t *nextTransitionWs =
		getNextTransition(currentScene, targetScene);
	obs_weak_source_release(currentScene);

	obs_source_t *nextTransition = nullptr;
	if (nextTransitionWs) {
		nextTransition = obs_weak_source_get_source(nextTransitionWs);
	} else if (transition) {
		nextTransition = obs_weak_source_get_source(transition);
	}

	if (nextTransition) {
		obs_frontend_set_current_transition(nextTransition);
	}

	if (transitionOverrideOverride)
		overwriteTransitionOverride(targetScene, nextTransition, td);

	obs_weak_source_release(nextTransitionWs);
	obs_source_release(nextTransition);
}

void SwitcherData::saveSceneTransitions(obs_data_t *obj)
{
	obs_data_array_t *sceneTransitionsArray = obs_data_array_create();
	for (SceneTransition &s : switcher->sceneTransitions) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source1 = obs_weak_source_get_source(s.scene);
		obs_source_t *source2 = obs_weak_source_get_source(s.scene2);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source1 && source2 && transition) {
			const char *sceneName1 = obs_source_get_name(source1);
			const char *sceneName2 = obs_source_get_name(source2);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "Scene1", sceneName1);
			obs_data_set_string(array_obj, "Scene2", sceneName2);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_array_push_back(sceneTransitionsArray,
						 array_obj);
			obs_source_release(source1);
			obs_source_release(source2);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneTransitions", sceneTransitionsArray);
	obs_data_array_release(sceneTransitionsArray);

	obs_data_array_t *defaultTransitionsArray = obs_data_array_create();
	for (DefaultSceneTransition &s : switcher->defaultSceneTransitions) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "Scene", sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_array_push_back(defaultTransitionsArray,
						 array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "defaultTransitions", defaultTransitionsArray);
	obs_data_array_release(defaultTransitionsArray);

	obs_data_set_bool(obj, "tansitionOverrideOverride",
			  switcher->tansitionOverrideOverride);
}

void SwitcherData::loadSceneTransitions(obs_data_t *obj)
{
	switcher->sceneTransitions.clear();

	obs_data_array_t *sceneTransitionsArray =
		obs_data_get_array(obj, "sceneTransitions");
	size_t count = obs_data_array_count(sceneTransitionsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(sceneTransitionsArray, i);

		const char *scene1 = obs_data_get_string(array_obj, "Scene1");
		const char *scene2 = obs_data_get_string(array_obj, "Scene2");
		const char *transition =
			obs_data_get_string(array_obj, "transition");

		switcher->sceneTransitions.emplace_back(
			GetWeakSourceByName(scene1),
			GetWeakSourceByName(scene2),
			GetWeakTransitionByName(transition));

		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneTransitionsArray);

	switcher->defaultSceneTransitions.clear();

	obs_data_array_t *defaultTransitionsArray =
		obs_data_get_array(obj, "defaultTransitions");
	count = obs_data_array_count(defaultTransitionsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(defaultTransitionsArray, i);

		const char *scene = obs_data_get_string(array_obj, "Scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");

		switcher->defaultSceneTransitions.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition));

		obs_data_release(array_obj);
	}
	obs_data_array_release(defaultTransitionsArray);

	switcher->tansitionOverrideOverride =
		obs_data_get_bool(obj, "tansitionOverrideOverride");
}

void SceneSwitcher::setupTransitionsTab()
{
	for (auto &s : switcher->sceneTransitions) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->sceneTransitions);
		ui->sceneTransitions->addItem(item);
		TransitionSwitchWidget *sw = new TransitionSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->sceneTransitions->setItemWidget(item, sw);
	}

	for (auto &s : switcher->defaultSceneTransitions) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->defaultTransitions);
		ui->defaultTransitions->addItem(item);
		DefTransitionSwitchWidget *sw =
			new DefTransitionSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->defaultTransitions->setItemWidget(item, sw);
	}

	ui->transitionOverridecheckBox->setChecked(
		switcher->tansitionOverrideOverride);
}

bool SceneTransition::initialized()
{
	return SceneSwitcherEntry::initialized() && scene2;
}

bool SceneTransition::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(scene2));
}

TransitionSwitchWidget::TransitionSwitchWidget(SceneTransition *s)
	: SwitchWidget(s, false)
{
	scenes2 = new QComboBox();

	switchLabel = new QLabel("Switch from scene");
	toLabel = new QLabel("to scene");
	usingLabel = new QLabel("using transition");

	QWidget::connect(scenes2, SIGNAL(currentTextChanged(const QString &)),
			 this, SLOT(Scene2Changed(const QString &)));

	SceneSwitcher::populateSceneSelection(scenes2, false);

	if (s) {
		scenes2->setCurrentText(GetWeakSourceName(s->scene2).c_str());
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(switchLabel);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(toLabel);
	mainLayout->addWidget(scenes2);
	mainLayout->addWidget(usingLabel);
	mainLayout->addWidget(transitions);
	mainLayout->addStretch();

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

SceneTransition *TransitionSwitchWidget::getSwitchData()
{
	return switchData;
}

void TransitionSwitchWidget::setSwitchData(SceneTransition *s)
{
	switchData = s;
}

void TransitionSwitchWidget::swapSwitchData(TransitionSwitchWidget *s1,
					    TransitionSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	SceneTransition *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void TransitionSwitchWidget::Scene2Changed(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->scene2 = GetWeakSourceByQString(text);
}

DefTransitionSwitchWidget::DefTransitionSwitchWidget(DefaultSceneTransition *s)
	: SwitchWidget(s, false)
{
	whenLabel = new QLabel("When scene");
	switchLabel =
		new QLabel("is active change default scene transition to ");

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(whenLabel);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(switchLabel);
	mainLayout->addWidget(transitions);
	mainLayout->addStretch();

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

DefaultSceneTransition *DefTransitionSwitchWidget::getSwitchData()
{
	return switchData;
}

void DefTransitionSwitchWidget::setSwitchData(DefaultSceneTransition *s)
{
	switchData = s;
}

void DefTransitionSwitchWidget::swapSwitchData(DefTransitionSwitchWidget *s1,
					       DefTransitionSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	DefaultSceneTransition *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}
