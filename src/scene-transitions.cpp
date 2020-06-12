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

		lock_guard<mutex> lock(switcher->m);
		switcher->sceneTransitions.emplace_back(
			source1, source2, transition,
			text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->sceneTransitions->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->sceneTransitions) {
				if (s.scene1 == source1 &&
				    s.scene2 == source2) {
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

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
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

		lock_guard<mutex> lock(switcher->m);
		switcher->defaultSceneTransitions.emplace_back(
			source, transition, text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->defaultTransitions->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
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

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
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

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->sceneTransitions) {
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) ==
		    0) {
			string scene1 = GetWeakSourceName(s.scene1);
			string scene2 = GetWeakSourceName(s.scene2);
			string transitionName = GetWeakSourceName(s.transition);
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

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->defaultSceneTransitions) {
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) ==
		    0) {
			string scene = GetWeakSourceName(s.scene);
			string transitionName = GetWeakSourceName(s.transition);
			ui->defaultTransitionsScene->setCurrentText(
				scene.c_str());
			ui->defaultTransitionsTransitions->setCurrentText(
				transitionName.c_str());
			break;
		}
	}
}

obs_weak_source_t *getNextTransition(obs_weak_source_t *scene1,
				     obs_weak_source_t *scene2)
{
	obs_weak_source_t *ws = nullptr;
	if (scene1 && scene2) {
		for (SceneTransition &t : switcher->sceneTransitions) {
			if (t.scene1 == scene1 && t.scene2 == scene2)
				ws = t.transition;
		}
		obs_weak_source_addref(ws);
	}
	return ws;
}
