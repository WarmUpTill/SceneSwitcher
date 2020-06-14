#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_timeSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->timeSwitches->item(idx);

	QString timeScenestr = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->timeSwitches) {
		if (timeScenestr.compare(s.timeSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->timeScenes->setCurrentText(sceneName);
			ui->timeEdit->setTime(s.time);
			ui->timeTransitions->setCurrentText(transitionName);
			break;
		}
	}
}

int SceneSwitcher::timeFindByData(const QString &timeStr)
{
	QRegExp rx("At " + timeStr + " switch to .*");
	int count = ui->timeSwitches->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->timeSwitches->item(i);
		QString str = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(str))
			return i;
	}

	return -1;
}

void SceneSwitcher::on_timeAdd_clicked()
{
	QString sceneName = ui->timeScenes->currentText();
	QString transitionName = ui->timeTransitions->currentText();
	QTime time = ui->timeEdit->time();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeTimeSwitchName(sceneName, transitionName, time);
	QVariant v = QVariant::fromValue(text);

	int idx = timeFindByData(time.toString());

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->timeSwitches.emplace_back(
			source, transition, time,
			(sceneName == QString(PREVIOUS_SCENE_NAME)),
			text.toUtf8().constData());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->timeSwitches);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->timeSwitches->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->timeSwitches) {
				if (s.time == time) {
					s.scene = source;
					s.transition = transition;
					s.usePreviousScene =
						(sceneName ==
						 QString(PREVIOUS_SCENE_NAME));
					s.timeSwitchStr =
						text.toUtf8().constData();
					;
					break;
				}
			}
		}

		ui->timeSwitches->sortItems();
	}
}

void SceneSwitcher::on_timeRemove_clicked()
{
	QListWidgetItem *item = ui->timeSwitches->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->timeSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.timeSwitchStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SwitcherData::checkTimeSwitch(bool &match, OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (timeSwitches.size() == 0)
		return;

	QTime now = QTime::currentTime();

	for (TimeSwitch &s : timeSwitches) {

		QTime validSwitchTimeWindow = s.time.addMSecs(interval);

		match = s.time <= now && now <= validSwitchTimeWindow;
		if (!match &&
		    validSwitchTimeWindow.msecsSinceStartOfDay() < interval) {
			// check for overflow
			match = now >= s.time || now <= validSwitchTimeWindow;
		}

		if (match) {
			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				blog(LOG_INFO,
				     "Advanced Scene Switcher time match");

			break;
		}
	}
}

void SwitcherData::saveTimeSwitches(obs_data_t *obj)
{
	obs_data_array_t *timeArray = obs_data_array_create();
	for (TimeSwitch &s : switcher->timeSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *sceneSource = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if ((s.usePreviousScene || sceneSource) && transition) {
			const char *sceneName =
				obs_source_get_name(sceneSource);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "scene",
					    s.usePreviousScene
						    ? PREVIOUS_SCENE_NAME
						    : sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_string(
				array_obj, "time",
				s.time.toString().toStdString().c_str());
			obs_data_array_push_back(timeArray, array_obj);
		}
		obs_source_release(sceneSource);
		obs_source_release(transition);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "timeSwitches", timeArray);
	obs_data_array_release(timeArray);
}

void SwitcherData::loadTimeSwitches(obs_data_t *obj)
{
	switcher->timeSwitches.clear();

	obs_data_array_t *timeArray = obs_data_get_array(obj, "timeSwitches");
	size_t count = obs_data_array_count(timeArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(timeArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		QTime time = QTime::fromString(
			obs_data_get_string(array_obj, "time"));

		std::string timeSwitchStr =
			MakeTimeSwitchName(scene, transition, time)
				.toUtf8()
				.constData();

		switcher->timeSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), time,
			(strcmp(scene, PREVIOUS_SCENE_NAME) == 0),
			timeSwitchStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(timeArray);
}
