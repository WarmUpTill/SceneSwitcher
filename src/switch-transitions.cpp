#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_transitionsAdd_clicked()
{
	QString scene1Name = ui->transitionsScene1->currentText();
	QString scene2Name = ui->transitionsScene2->currentText();
	QString transitionName = ui->transitionsTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text =
		MakeSceneTransitionName(scene1Name, scene2Name, transitionName);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneTransitionsFindByData(scene1Name, scene2Name);

	if (idx == -1) {
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->sceneTransitions.emplace_back(
			source1, source2, transition,
			text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->sceneTransitions->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->sceneTransitions) {
				if (s.scene == source1 && s.scene2 == source2) {
					s.transition = transition;
					s.sceneTransitionStr =
						text.toUtf8().constData();
					break;
				}
			}
		}

		ui->sceneTransitions->sortItems();
	}
}

void SceneSwitcher::on_transitionsRemove_clicked()
{
	QListWidgetItem *item = ui->sceneTransitions->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->sceneTransitions;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.sceneTransitionStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_defaultTransitionsAdd_clicked()
{
	QString sceneName = ui->defaultTransitionsScene->currentText();
	QString transitionName =
		ui->defaultTransitionsTransitions->currentText();

	if (sceneName.isEmpty() || transitionName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text =
		MakeDefaultSceneTransitionName(sceneName, transitionName);
	QVariant v = QVariant::fromValue(text);

	int idx = DefaultTransitionsFindByData(sceneName);

	if (idx == -1) {
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->defaultTransitions);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->defaultSceneTransitions.emplace_back(
			source, transition, text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->defaultTransitions->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->defaultSceneTransitions) {
				if (s.scene == source) {
					s.transition = transition;
					s.sceneTransitionStr =
						text.toUtf8().constData();
					break;
				}
			}
		}

		ui->defaultTransitions->sortItems();
	}
}

void SceneSwitcher::on_defaultTransitionsRemove_clicked()
{
	QListWidgetItem *item = ui->defaultTransitions->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->defaultSceneTransitions;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.sceneTransitionStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
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

int SceneSwitcher::SceneTransitionsFindByData(const QString &scene1,
					      const QString &scene2)
{
	QRegExp rx(scene1 + " --- .* --> " + scene2);
	int count = ui->sceneTransitions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->sceneTransitions->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString)) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::DefaultTransitionsFindByData(const QString &scene)
{
	QRegExp rx(scene + " --> .*");
	int count = ui->defaultTransitions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->defaultTransitions->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString)) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_sceneTransitions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->sceneTransitions->item(idx);

	QString sceneTransition = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->sceneTransitions) {
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) ==
		    0) {
			std::string scene1 = GetWeakSourceName(s.scene);
			std::string scene2 = GetWeakSourceName(s.scene2);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			ui->transitionsScene1->setCurrentText(scene1.c_str());
			ui->transitionsScene2->setCurrentText(scene2.c_str());
			ui->transitionsTransitions->setCurrentText(
				transitionName.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_defaultTransitions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->defaultTransitions->item(idx);

	QString sceneTransition = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->defaultSceneTransitions) {
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) ==
		    0) {
			std::string scene = GetWeakSourceName(s.scene);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			ui->defaultTransitionsScene->setCurrentText(
				scene.c_str());
			ui->defaultTransitionsTransitions->setCurrentText(
				transitionName.c_str());
			break;
		}
	}
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

		std::string sceneTransitionsStr =
			MakeSceneTransitionName(scene1, scene2, transition)
				.toUtf8()
				.constData();

		switcher->sceneTransitions.emplace_back(
			GetWeakSourceByName(scene1),
			GetWeakSourceByName(scene2),
			GetWeakTransitionByName(transition),
			sceneTransitionsStr);

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

		std::string sceneTransitionsStr =
			MakeDefaultSceneTransitionName(scene, transition)
				.toUtf8()
				.constData();

		switcher->defaultSceneTransitions.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition),
			sceneTransitionsStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(defaultTransitionsArray);

	switcher->tansitionOverrideOverride =
		obs_data_get_bool(obj, "tansitionOverrideOverride");
}

void SceneSwitcher::setupTransitionsTab()
{
	populateSceneSelection(ui->transitionsScene1, false);
	populateSceneSelection(ui->transitionsScene2, false);
	populateSceneSelection(ui->defaultTransitionsScene, false);
	populateTransitionSelection(ui->transitionsTransitions);
	populateTransitionSelection(ui->defaultTransitionsTransitions);

	for (auto &s : switcher->sceneTransitions) {
		std::string sceneName1 = GetWeakSourceName(s.scene);
		std::string sceneName2 = GetWeakSourceName(s.scene2);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneTransitionName(sceneName1.c_str(),
						       sceneName2.c_str(),
						       transitionName.c_str());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, text);
	}

	for (auto &s : switcher->defaultSceneTransitions) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeDefaultSceneTransitionName(
			sceneName.c_str(), transitionName.c_str());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->defaultTransitions);
		item->setData(Qt::UserRole, text);
	}

	ui->transitionOverridecheckBox->setChecked(
		switcher->tansitionOverrideOverride);
}

static inline QString MakeSceneTransitionName(const QString &scene1,
					      const QString &scene2,
					      const QString &transition)
{
	return scene1 + QStringLiteral(" --- ") + transition +
	       QStringLiteral(" --> ") + scene2;
}

static inline QString MakeDefaultSceneTransitionName(const QString &scene,
						     const QString &transition)
{
	return scene + QStringLiteral(" --> ") + transition;
}
