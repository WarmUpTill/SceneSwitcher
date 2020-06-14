#include <QFileDialog>
#include <QTextStream>
#include <obs.hpp>

#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_sceneRoundTripAdd_clicked()
{
	QString scene1Name = ui->sceneRoundTripScenes1->currentText();
	QString scene2Name = ui->sceneRoundTripScenes2->currentText();
	QString transitionName = ui->sceneRoundTripTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	double delay = ui->sceneRoundTripSpinBox->value();

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeSceneRoundTripSwitchName(scene1Name, scene2Name,
						    transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneRoundTripFindByData(scene1Name);

	if (idx == -1) {
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->sceneRoundTripSwitches.emplace_back(
			source1, source2, transition, int(delay * 1000),
			(scene2Name == QString(PREVIOUS_SCENE_NAME)),
			text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->sceneRoundTrips->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->sceneRoundTripSwitches) {
				if (s.scene1 == source1) {
					s.scene2 = source2;
					s.delay = int(delay * 1000);
					s.transition = transition;
					s.usePreviousScene =
						(scene2Name ==
						 QString(PREVIOUS_SCENE_NAME));
					s.sceneRoundTripStr =
						text.toUtf8().constData();
					break;
				}
			}
		}

		ui->sceneRoundTrips->sortItems();
	}
}

void SceneSwitcher::on_sceneRoundTripRemove_clicked()
{
	QListWidgetItem *item = ui->sceneRoundTrips->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->sceneRoundTripSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.sceneRoundTripStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_autoStopSceneCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		ui->autoStopScenes->setDisabled(true);
		switcher->autoStopEnable = false;
	} else {
		ui->autoStopScenes->setDisabled(false);
		switcher->autoStopEnable = true;
	}
}

void SceneSwitcher::UpdateAutoStopScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->autoStopScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_autoStopScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateAutoStopScene(text);
}

void SceneSwitcher::on_sceneRoundTripSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Save Scene Round Trip to file ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty()) {
		QFile file(directory);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		for (SceneRoundTripSwitch s :
		     switcher->sceneRoundTripSwitches) {
			out << QString::fromStdString(
				       GetWeakSourceName(s.scene1))
			    << "\n";
			if (s.usePreviousScene)
				out << (PREVIOUS_SCENE_NAME) << "\n";
			else
				out << QString::fromStdString(
					       GetWeakSourceName(s.scene2))
				    << "\n";
			out << s.delay << "\n";
			out << QString::fromStdString(s.sceneRoundTripStr)
			    << "\n";
			out << QString::fromStdString(
				       GetWeakSourceName(s.transition))
			    << "\n";
		}
	}
}

void SceneSwitcher::on_sceneRoundTripLoad_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read Scene Round Trip from ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty()) {
		QFile file(directory);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;

		QTextStream in(&file);
		std::vector<QString> lines;

		std::vector<SceneRoundTripSwitch> newSceneRoundTripSwitch;

		while (!in.atEnd()) {
			QString line = in.readLine();
			lines.push_back(line);
			if (lines.size() == 5) {
				OBSWeakSource scene1 =
					GetWeakSourceByQString(lines[0]);
				OBSWeakSource scene2 =
					GetWeakSourceByQString(lines[1]);
				OBSWeakSource transition =
					GetWeakTransitionByQString(lines[4]);

				if (WeakSourceValid(scene1) &&
				    ((lines[1] ==
				      QString(PREVIOUS_SCENE_NAME)) ||
				     (WeakSourceValid(scene2))) &&
				    WeakSourceValid(transition)) {
					newSceneRoundTripSwitch.emplace_back(
						SceneRoundTripSwitch(
							GetWeakSourceByQString(
								lines[0]),
							GetWeakSourceByQString(
								lines[1]),
							GetWeakTransitionByQString(
								lines[4]),
							lines[2].toInt(),
							(lines[1] ==
							 QString(PREVIOUS_SCENE_NAME)),
							lines[3].toStdString()));
				}
				lines.clear();
			}
		}

		if (lines.size() != 0 || newSceneRoundTripSwitch.size() == 0)
			return;

		switcher->sceneRoundTripSwitches.clear();
		ui->sceneRoundTrips->clear();
		switcher->sceneRoundTripSwitches = newSceneRoundTripSwitch;
		for (SceneRoundTripSwitch s :
		     switcher->sceneRoundTripSwitches) {
			QListWidgetItem *item = new QListWidgetItem(
				QString::fromStdString(s.sceneRoundTripStr),
				ui->sceneRoundTrips);
			item->setData(
				Qt::UserRole,
				QString::fromStdString(s.sceneRoundTripStr));
		}
	}
}

void SwitcherData::checkSceneRoundTrip(bool &match, OBSWeakSource &scene,
				       OBSWeakSource &transition,
				       std::unique_lock<std::mutex> &lock)
{
	bool sceneRoundTripActive = false;
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (SceneRoundTripSwitch &s : sceneRoundTripSwitches) {
		if (s.scene1 == ws) {
			sceneRoundTripActive = true;
			int dur = s.delay - interval;
			if (dur > 0) {
				waitScene = currentSource;

				if (verbose)
					blog(LOG_INFO,
					     "Advanced Scene Switcher sequence sleep %d",
					     dur);

				cv.wait_for(lock,
					    std::chrono::milliseconds(dur));
			}
			obs_source_t *currentSource2 =
				obs_frontend_get_current_scene();

			// only switch if user hasn't changed scene manually
			if (currentSource == currentSource2) {
				match = true;
				scene = (s.usePreviousScene) ? previousScene
							     : s.scene2;
				transition = s.transition;
			}
			obs_source_release(currentSource2);
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SceneSwitcher::on_sceneRoundTrips_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->sceneRoundTrips->item(idx);

	QString sceneRoundTrip = item->text();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->sceneRoundTripSwitches) {
		if (sceneRoundTrip.compare(s.sceneRoundTripStr.c_str()) == 0) {
			std::string scene1 = GetWeakSourceName(s.scene1);
			std::string scene2 = GetWeakSourceName(s.scene2);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			int delay = s.delay;
			ui->sceneRoundTripScenes1->setCurrentText(
				scene1.c_str());
			ui->sceneRoundTripScenes2->setCurrentText(
				scene2.c_str());
			ui->sceneRoundTripTransitions->setCurrentText(
				transitionName.c_str());
			ui->sceneRoundTripSpinBox->setValue((double)delay /
							    1000);
			break;
		}
	}
}

int SceneSwitcher::SceneRoundTripFindByData(const QString &scene1)
{
	QRegExp rx(scene1 + " ->.*");
	int count = ui->sceneRoundTrips->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->sceneRoundTrips->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString)) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SwitcherData::autoStopStreamAndRecording()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	if (ws && autoStopScene == ws) {
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		if (obs_frontend_recording_active())
			obs_frontend_recording_stop();
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SwitcherData::saveSceneRoundTripSwitches(obs_data_t *obj)
{
	obs_data_array_t *sceneRoundTripArray = obs_data_array_create();
	for (SceneRoundTripSwitch &s : switcher->sceneRoundTripSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source1 = obs_weak_source_get_source(s.scene1);
		obs_source_t *source2 = obs_weak_source_get_source(s.scene2);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source1 && (s.usePreviousScene || source2) && transition) {
			const char *sceneName1 = obs_source_get_name(source1);
			const char *sceneName2 = obs_source_get_name(source2);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "sceneRoundTripScene1",
					    sceneName1);
			obs_data_set_string(array_obj, "sceneRoundTripScene2",
					    s.usePreviousScene
						    ? PREVIOUS_SCENE_NAME
						    : sceneName2);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_int(
				array_obj, "sceneRoundTripDelay",
				s.delay /
					1000); //delay stored in two separate values
			obs_data_set_int(
				array_obj, "sceneRoundTripDelayMs",
				s.delay %
					1000); //to be compatible with older versions
			obs_data_set_string(array_obj, "sceneRoundTripStr",
					    s.sceneRoundTripStr.c_str());
			obs_data_array_push_back(sceneRoundTripArray,
						 array_obj);
			obs_source_release(source1);
			obs_source_release(source2);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneRoundTrip", sceneRoundTripArray);
	obs_data_array_release(sceneRoundTripArray);
}

void SwitcherData::loadSceneRoundTripSwitches(obs_data_t *obj)
{
	switcher->sceneRoundTripSwitches.clear();

	obs_data_array_t *sceneRoundTripArray =
		obs_data_get_array(obj, "sceneRoundTrip");
	size_t count = obs_data_array_count(sceneRoundTripArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(sceneRoundTripArray, i);

		const char *scene1 =
			obs_data_get_string(array_obj, "sceneRoundTripScene1");
		const char *scene2 =
			obs_data_get_string(array_obj, "sceneRoundTripScene2");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		int delay = obs_data_get_int(
			array_obj,
			"sceneRoundTripDelay"); //delay stored in two separate values
		delay = delay * 1000 +
			obs_data_get_int(
				array_obj,
				"sceneRoundTripDelayMs"); //to be compatible with older versions
		std::string str =
			MakeSceneRoundTripSwitchName(scene1, scene2, transition,
						     ((double)delay) / 1000.0)
				.toUtf8()
				.constData();
		const char *sceneRoundTripStr = str.c_str();

		switcher->sceneRoundTripSwitches.emplace_back(
			GetWeakSourceByName(scene1),
			GetWeakSourceByName(scene2),
			GetWeakTransitionByName(transition), delay,
			(strcmp(scene2, PREVIOUS_SCENE_NAME) == 0),
			sceneRoundTripStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneRoundTripArray);
}
