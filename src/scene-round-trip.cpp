#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>
#include <obs.hpp>

#include "headers/advanced-scene-switcher.hpp"

#define SECONDS_INDEX 0
#define MINUTES_INDEX 1
#define HOURS_INDEX 2

void SceneSwitcher::on_sceneRoundTripDelayUnits_currentIndexChanged(int index)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	double val = ui->sceneRoundTripSpinBox->value();

	switch (switcher->sceneRoundTripUnitMultiplier) {
	case 1:
		val /= pow(60, index);
		break;
	case 60:
		val /= pow(60, index - 1);
		break;
	case 3600:
		val /= pow(60, index - 2);
		break;
	}

	ui->sceneRoundTripSpinBox->setValue(val);

	switch (index) {
	case SECONDS_INDEX:
		switcher->sceneRoundTripUnitMultiplier = 1;
		break;
	case MINUTES_INDEX:
		switcher->sceneRoundTripUnitMultiplier = 60;
		break;
	case HOURS_INDEX:
		switcher->sceneRoundTripUnitMultiplier = 3600;
		break;
	}
}

void SceneSwitcher::on_sceneRoundTripAdd_clicked()
{
	QString scene1Name = ui->sceneRoundTripScenes1->currentText();
	QString scene2Name = ui->sceneRoundTripScenes2->currentText();
	QString transitionName = ui->sceneRoundTripTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	double delay = ui->sceneRoundTripSpinBox->value() *
		       switcher->sceneRoundTripUnitMultiplier;

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
			source1, source2, transition, delay,
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
					s.delay = delay;
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

void SceneSwitcher::on_sceneRoundTripSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Save Scene Round Trip to file ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (directory.isEmpty())
		return;
	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create();
	switcher->saveSceneRoundTripSwitches(obj);
	obs_data_save_json(obj, file.fileName().toUtf8().constData());
	obs_data_release(obj);
}

void SceneSwitcher::on_sceneRoundTripLoad_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read Scene Round Trip from ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (directory.isEmpty())
		return;

	QFile file(directory);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create_from_json_file(
		file.fileName().toUtf8().constData());

	if (!obj) {
		QMessageBox Msgbox;
		Msgbox.setText(
			"Advanced Scene Switcher failed to import settings (try import with previous version of the plugin)");
		Msgbox.exec();
		return;
	}
	switcher->loadSceneRoundTripSwitches(obj);
	obs_data_release(obj);

	QMessageBox Msgbox;
	Msgbox.setText(
		"Advanced Scene Switcher settings imported successfully");
	Msgbox.exec();
	close();
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
			int dur = s.delay * 1000 - interval;
			if (dur > 0) {
				waitScene = currentSource;

				if (verbose)
					blog(LOG_INFO,
					     "Advanced Scene Switcher sequence sleep %d",
					     dur);

				cv.wait_for(lock,
					    std::chrono::milliseconds(dur));
				waitScene = nullptr;
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
			double delay = s.delay /
				       switcher->sceneRoundTripUnitMultiplier;
			ui->sceneRoundTripScenes1->setCurrentText(
				scene1.c_str());
			ui->sceneRoundTripScenes2->setCurrentText(
				scene2.c_str());
			ui->sceneRoundTripTransitions->setCurrentText(
				transitionName.c_str());
			ui->sceneRoundTripSpinBox->setValue(delay);
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

void SceneSwitcher::on_sceneRoundTripUp_clicked()
{
	int index = ui->sceneRoundTrips->currentRow();
	if (index != -1 && index != 0) {
		ui->sceneRoundTrips->insertItem(
			index - 1, ui->sceneRoundTrips->takeItem(index));
		ui->sceneRoundTrips->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneRoundTripSwitches.begin() + index,
			  switcher->sceneRoundTripSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_sceneRoundTripDown_clicked()
{
	int index = ui->sceneRoundTrips->currentRow();
	if (index != -1 && index != ui->sceneRoundTrips->count() - 1) {
		ui->sceneRoundTrips->insertItem(
			index + 1, ui->sceneRoundTrips->takeItem(index));
		ui->sceneRoundTrips->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneRoundTripSwitches.begin() + index,
			  switcher->sceneRoundTripSwitches.begin() + index + 1);
	}
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
			obs_data_set_double(array_obj, "delay", s.delay);
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

		double delay = 0;

		// To be removed in future version
		// to be compatible with older versions
		if (!obs_data_has_user_value(array_obj, "delay")) {
			delay = obs_data_get_int(array_obj,
						 "sceneRoundTripDelay");
			delay = delay * 1000 +
				obs_data_get_int(array_obj,
						 "sceneRoundTripDelayMs");
		} else {
			delay = obs_data_get_double(array_obj, "delay");
		}

		std::string str = MakeSceneRoundTripSwitchName(
					  scene1, scene2, transition, delay)
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
void SceneSwitcher::setupSequenceTab()
{
	populateSceneSelection(ui->sceneRoundTripScenes1, false);
	populateSceneSelection(ui->sceneRoundTripScenes2, true);
	populateTransitionSelection(ui->sceneRoundTripTransitions);

	int smallestDelay = switcher->interval;
	for (auto &s : switcher->sceneRoundTripSwitches) {
		std::string sceneName1 = GetWeakSourceName(s.scene1);
		std::string sceneName2 = (s.usePreviousScene)
						 ? PREVIOUS_SCENE_NAME
						 : GetWeakSourceName(s.scene2);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneRoundTripSwitchName(
			sceneName1.c_str(), sceneName2.c_str(),
			transitionName.c_str(), s.delay);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, text);

		if (s.delay < smallestDelay)
			smallestDelay = s.delay;
	}
	(smallestDelay * 1000 < switcher->interval)
		? ui->intervalWarning->setVisible(true)
		: ui->intervalWarning->setVisible(false);

	ui->sceneRoundTripDelayUnits->addItem("seconds");
	ui->sceneRoundTripDelayUnits->addItem("minutes");
	ui->sceneRoundTripDelayUnits->addItem("hours");

	switch (switcher->sceneRoundTripUnitMultiplier) {
	case 1:
		ui->sceneRoundTripDelayUnits->setCurrentIndex(0);
		break;
	case 60:
		ui->sceneRoundTripDelayUnits->setCurrentIndex(1);
		break;
	case 3600:
		ui->sceneRoundTripDelayUnits->setCurrentIndex(2);
		break;
	default:
		break;
	}
}
