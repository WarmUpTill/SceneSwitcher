#include <QFileDialog>
#include <QTextStream>
#include <QMessageBox>

#include "headers/advanced-scene-switcher.hpp"

constexpr auto seconds_index = 0;
constexpr auto minutes_index = 1;
constexpr auto hours_index = 2;

void SceneSwitcher::on_sceneSequenceDelayUnits_currentIndexChanged(int index)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);

	double val = ui->sceneSequenceSpinBox->value();

	switch (switcher->sceneSequenceMultiplier) {
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

	ui->sceneSequenceSpinBox->setValue(val);

	switch (index) {
	case seconds_index:
		switcher->sceneSequenceMultiplier = 1;
		break;
	case minutes_index:
		switcher->sceneSequenceMultiplier = 60;
		break;
	case hours_index:
		switcher->sceneSequenceMultiplier = 3600;
		break;
	}
}

void SceneSwitcher::on_sceneSequenceAdd_clicked()
{
	QString scene1Name = ui->sceneSequenceScenes1->currentText();
	QString scene2Name = ui->sceneSequenceScenes2->currentText();
	QString transitionName = ui->sceneSequenceTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	double delay = ui->sceneSequenceSpinBox->value() *
		       switcher->sceneSequenceMultiplier;

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeSceneSequenceSwitchName(scene1Name, scene2Name,
						    transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneSequenceFindByData(scene1Name);

	if (idx == -1) {
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneSequences);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->sceneSequenceSwitches.emplace_back(
			source1, source2, transition, delay,
			(scene2Name == QString(previous_scene_name)),
			text.toUtf8().constData());
	} else {
		QListWidgetItem *item = ui->sceneSequences->item(idx);
		item->setText(text);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->sceneSequenceSwitches) {
				if (s.startScene == source1) {
					s.scene = source2;
					s.delay = delay;
					s.transition = transition;
					s.usePreviousScene =
						(scene2Name ==
						 QString(previous_scene_name));
					s.sceneSequenceStr =
						text.toUtf8().constData();
					break;
				}
			}
		}
	}
}

void SceneSwitcher::on_sceneSequenceRemove_clicked()
{
	QListWidgetItem *item = ui->sceneSequences->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &switches = switcher->sceneSequenceSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.sceneSequenceStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_sceneSequenceSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Save Scene Sequence to file ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (directory.isEmpty())
		return;
	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create();
	switcher->saveSceneSequenceSwitches(obj);
	obs_data_save_json(obj, file.fileName().toUtf8().constData());
	obs_data_release(obj);
}

// to be removed once support for old file format is dropped
bool oldSceneRoundTripLoad(QFile *file)
{
	QTextStream in(file);
	std::vector<QString> lines;

	std::vector<SceneSequenceSwitch> newSceneRoundTripSwitch;

	while (!in.atEnd()) {
		QString line = in.readLine();
		lines.push_back(line);
		if (lines.size() == 5) {
			OBSWeakSource scene1 = GetWeakSourceByQString(lines[0]);
			OBSWeakSource scene2 = GetWeakSourceByQString(lines[1]);
			OBSWeakSource transition =
				GetWeakTransitionByQString(lines[4]);

			if (WeakSourceValid(scene1) &&
			    ((lines[1] == QString(previous_scene_name)) ||
			     (WeakSourceValid(scene2))) &&
			    WeakSourceValid(transition)) {
				newSceneRoundTripSwitch.emplace_back(
					SceneSequenceSwitch(
						GetWeakSourceByQString(lines[0]),
						GetWeakSourceByQString(lines[1]),
						GetWeakTransitionByQString(
							lines[4]),
						lines[2].toDouble() / 1000.,
						(lines[1] ==
						 QString(previous_scene_name)),
						lines[3].toStdString()));
			}
			lines.clear();
		}
	}

	if (lines.size() != 0 || newSceneRoundTripSwitch.size() == 0)
		return false;

	switcher->sceneSequenceSwitches.clear();
	switcher->sceneSequenceSwitches = newSceneRoundTripSwitch;
	return true;
}

void SceneSwitcher::on_sceneSequenceLoad_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read Scene Sequence from ..."),
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
		QString txt =
			"Advanced Scene Switcher failed to import settings!\n";
		txt += "Falling back to old format.";
		Msgbox.setText(txt);
		Msgbox.exec();
		if (oldSceneRoundTripLoad(&file)) {
			QString txt =
				"Advanced Scene Switcher settings imported successfully!\n";
			txt += "Please resave settings as support for old format will be dropped in next version.";
			Msgbox.setText(txt);
			Msgbox.exec();
			close();
		} else {
			Msgbox.setText(
				"Advanced Scene Switcher failed to import settings!");
			Msgbox.exec();
		}
		return;
	}
	switcher->loadSceneSequenceSwitches(obj);
	obs_data_release(obj);

	QMessageBox Msgbox;
	Msgbox.setText(
		"Advanced Scene Switcher settings imported successfully!");
	Msgbox.exec();
	close();
}

void SwitcherData::checkSceneSequence(bool &match, OBSWeakSource &scene,
				       OBSWeakSource &transition,
				       std::unique_lock<std::mutex> &lock)
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (SceneSequenceSwitch &s : sceneSequenceSwitches) {
		if (s.startScene == ws) {
			int dur = s.delay * 1000 - interval;
			if (dur > 0) {
				waitScene = currentSource;

				if (verbose)
					s.logSleep(dur);

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
							     : s.scene;
				transition = s.transition;
				if (verbose)
					s.logMatch();
			}
			obs_source_release(currentSource2);
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void SceneSwitcher::on_sceneSequences_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->sceneSequences->item(idx);

	QString sceneSequence = item->text();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->sceneSequenceSwitches) {
		if (sceneSequence.compare(s.sceneSequenceStr.c_str()) == 0) {
			std::string scene1 = GetWeakSourceName(s.startScene);
			std::string scene2 = GetWeakSourceName(s.scene);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			double delay = s.delay /
				       switcher->sceneSequenceMultiplier;
			ui->sceneSequenceScenes1->setCurrentText(
				scene1.c_str());
			ui->sceneSequenceScenes2->setCurrentText(
				scene2.c_str());
			ui->sceneSequenceTransitions->setCurrentText(
				transitionName.c_str());
			ui->sceneSequenceSpinBox->setValue(delay);
			break;
		}
	}
}

int SceneSwitcher::SceneSequenceFindByData(const QString &scene1)
{
	QRegExp rx(scene1 + " ->.*");
	int count = ui->sceneSequences->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->sceneSequences->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString)) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_sceneSequenceUp_clicked()
{
	int index = ui->sceneSequences->currentRow();
	if (index != -1 && index != 0) {
		ui->sceneSequences->insertItem(
			index - 1, ui->sceneSequences->takeItem(index));
		ui->sceneSequences->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneSequenceSwitches.begin() + index,
			  switcher->sceneSequenceSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_sceneSequenceDown_clicked()
{
	int index = ui->sceneSequences->currentRow();
	if (index != -1 && index != ui->sceneSequences->count() - 1) {
		ui->sceneSequences->insertItem(
			index + 1, ui->sceneSequences->takeItem(index));
		ui->sceneSequences->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->sceneSequenceSwitches.begin() + index,
			  switcher->sceneSequenceSwitches.begin() + index + 1);
	}
}

void SwitcherData::saveSceneSequenceSwitches(obs_data_t *obj)
{
	obs_data_array_t *sceneSequenceArray = obs_data_array_create();
	for (SceneSequenceSwitch &s : switcher->sceneSequenceSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source1 =
			obs_weak_source_get_source(s.startScene);
		obs_source_t *source2 = obs_weak_source_get_source(s.scene);
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
						    ? previous_scene_name
						    : sceneName2);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_double(array_obj, "delay", s.delay);
			obs_data_array_push_back(sceneSequenceArray,
						 array_obj);
			obs_source_release(source1);
			obs_source_release(source2);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "sceneRoundTrip", sceneSequenceArray);
	obs_data_array_release(sceneSequenceArray);
}

void SwitcherData::loadSceneSequenceSwitches(obs_data_t *obj)
{
	switcher->sceneSequenceSwitches.clear();

	obs_data_array_t *sceneSequenceArray =
		obs_data_get_array(obj, "sceneRoundTrip");
	size_t count = obs_data_array_count(sceneSequenceArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(sceneSequenceArray, i);

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
			delay = double(obs_data_get_int(array_obj,
							"sceneRoundTripDelay"));
			delay = delay * 1000. +
				double(obs_data_get_int(
					array_obj, "sceneRoundTripDelayMs"));
			delay /= 1000.;
		} else {
			delay = obs_data_get_double(array_obj, "delay");
		}

		std::string str = MakeSceneSequenceSwitchName(
					  scene1, scene2, transition, delay)
					  .toUtf8()
					  .constData();
		const char *sceneSequenceStr = str.c_str();

		switcher->sceneSequenceSwitches.emplace_back(
			GetWeakSourceByName(scene1),
			GetWeakSourceByName(scene2),
			GetWeakTransitionByName(transition), delay,
			(strcmp(scene2, previous_scene_name) == 0),
			sceneSequenceStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(sceneSequenceArray);
}

void SceneSwitcher::setupSequenceTab()
{
	populateSceneSelection(ui->sceneSequenceScenes1, false);
	populateSceneSelection(ui->sceneSequenceScenes2, true);
	populateTransitionSelection(ui->sceneSequenceTransitions);

	double smallestDelay = double(switcher->interval) / 1000;
	for (auto &s : switcher->sceneSequenceSwitches) {
		std::string sceneName1 = GetWeakSourceName(s.startScene);
		std::string sceneName2 = (s.usePreviousScene)
						 ? previous_scene_name
						 : GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneSequenceSwitchName(
			sceneName1.c_str(), sceneName2.c_str(),
			transitionName.c_str(), s.delay);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneSequences);
		item->setData(Qt::UserRole, text);

		if (s.delay < smallestDelay)
			smallestDelay = s.delay;
	}
	(smallestDelay * 1000 < switcher->interval)
		? ui->intervalWarning->setVisible(true)
		: ui->intervalWarning->setVisible(false);

	ui->sceneSequenceDelayUnits->addItem("seconds");
	ui->sceneSequenceDelayUnits->addItem("minutes");
	ui->sceneSequenceDelayUnits->addItem("hours");

	switch (switcher->sceneSequenceMultiplier) {
	case 1:
		ui->sceneSequenceDelayUnits->setCurrentIndex(0);
		break;
	case 60:
		ui->sceneSequenceDelayUnits->setCurrentIndex(1);
		break;
	case 3600:
		ui->sceneSequenceDelayUnits->setCurrentIndex(2);
		break;
	default:
		break;
	}
}

static inline QString MakeSceneSequenceSwitchName(const QString &scene1,
						   const QString &scene2,
						   const QString &transition,
						   double delay)
{
	return scene1 + QStringLiteral(" -> wait for ") +
	       QString::number(delay) + QStringLiteral(" seconds -> ") +
	       scene2 + QStringLiteral(" (using ") + transition +
	       QStringLiteral(" transition)");
}
