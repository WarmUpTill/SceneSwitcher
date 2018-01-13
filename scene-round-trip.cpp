#include <QFileDialog>
#include <QTextStream>
#include <obs.hpp>

#include "advanced-scene-switcher.hpp"

obs_weak_source_t* getNextTransition(obs_weak_source_t* scene1, obs_weak_source_t* scene2);

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

	QString text = MakeSceneRoundTripSwitchName(scene1Name, scene2Name, transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneRoundTripFindByData(scene1Name);

	if (idx == -1)
	{
		QListWidgetItem* item = new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->sceneRoundTripSwitches.emplace_back(
			source1, source2, transition, int(delay * 1000), text.toUtf8().constData());
	}
	else
	{
		QListWidgetItem* item = ui->sceneRoundTrips->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->sceneRoundTripSwitches)
			{
				if (s.scene1 == source1)
				{
					s.scene2 = source2;
					s.delay = int(delay * 1000);
					s.transition = transition;
					s.sceneRoundTripStr = text.toUtf8().constData();
					break;
				}
			}
		}

		ui->sceneRoundTrips->sortItems();
	}
}

void SceneSwitcher::on_sceneRoundTripRemove_clicked()
{
	QListWidgetItem* item = ui->sceneRoundTrips->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->sceneRoundTripSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.sceneRoundTripStr == text)
			{
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

	lock_guard<mutex> lock(switcher->m);
	if (!state)
	{
		ui->autoStopScenes->setDisabled(true);
		switcher->autoStopEnable = false;
	}
	else
	{
		ui->autoStopScenes->setDisabled(false);
		switcher->autoStopEnable = true;
	}
}

void SceneSwitcher::UpdateAutoStopScene(const QString& name)
{
	obs_source_t* scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t* ws = obs_source_get_weak_source(scene);

	switcher->autoStopScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_autoStopScenes_currentTextChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateAutoStopScene(text);
}

void SceneSwitcher::on_sceneRoundTripSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Save Scene Round Trip to file ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
	{
		QFile file(directory);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		for (SceneRoundTripSwitch s : switcher->sceneRoundTripSwitches)
		{
			out << QString::fromStdString(GetWeakSourceName(s.scene1)) << "\n";
			out << QString::fromStdString(GetWeakSourceName(s.scene2)) << "\n";
			out << s.delay << "\n";
			out << QString::fromStdString(s.sceneRoundTripStr) << "\n";
			out << QString::fromStdString(GetWeakSourceName(s.transition)) << "\n";
		}
	}
}

void SceneSwitcher::on_sceneRoundTripLoad_clicked()
{
	lock_guard<mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read Scene Round Trip from ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
	{
		QFile file(directory);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;

		QTextStream in(&file);
		vector<QString> lines;

		vector<SceneRoundTripSwitch> newSceneRoundTripSwitch;

		while (!in.atEnd())
		{
			QString line = in.readLine();
			lines.push_back(line);
			if (lines.size() == 5)
			{
				OBSWeakSource scene1 = GetWeakSourceByQString(lines[0]);
				OBSWeakSource scene2 = GetWeakSourceByQString(lines[1]);
				OBSWeakSource transition = GetWeakTransitionByQString(lines[4]);

				if (WeakSourceValid(scene1) && WeakSourceValid(scene2)
					&& WeakSourceValid(transition))
				{
					newSceneRoundTripSwitch.emplace_back(SceneRoundTripSwitch(
						GetWeakSourceByQString(lines[0]),
						GetWeakSourceByQString(lines[1]),
						GetWeakTransitionByQString(lines[4]),
						lines[2].toInt(),
						lines[3].toStdString()));
				}
				lines.clear();
			}
		}
		//unvalid amount of lines in file or nothing valid read
		if (lines.size() != 0 || newSceneRoundTripSwitch.size() == 0)
			return;

		switcher->sceneRoundTripSwitches.clear();
		ui->sceneRoundTrips->clear();
		switcher->sceneRoundTripSwitches = newSceneRoundTripSwitch;
		for (SceneRoundTripSwitch s : switcher->sceneRoundTripSwitches)
		{
			QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(s.sceneRoundTripStr), ui->sceneRoundTrips);
			item->setData(Qt::UserRole, QString::fromStdString(s.sceneRoundTripStr));
		}
	}
}


void SwitcherData::checkSceneRoundTrip(bool& match, OBSWeakSource& scene, OBSWeakSource& transition, unique_lock<mutex>& lock)
{
	bool sceneRoundTripActive = false;
	obs_source_t* currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t* ws = obs_source_get_weak_source(currentSource);

	for (SceneRoundTripSwitch& s : sceneRoundTripSwitches)
	{
		if (s.scene1 == ws)
		{
			sceneRoundTripActive = true;
			int dur = s.delay - interval;
			if (dur > 0)
			{
				string s = obs_source_get_name(currentSource);
				waitSceneName = s;
				cv.wait_for(lock, chrono::milliseconds(dur));
			}
			obs_source_t* currentSource2 = obs_frontend_get_current_scene();

			// only switch if user hasn't changed scene manually
			if (currentSource == currentSource2)
			{
				match = true;
				scene = s.scene2;
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

	QListWidgetItem* item = ui->sceneRoundTrips->item(idx);

	QString sceneRoundTrip = item->text();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->sceneRoundTripSwitches)
	{
		if (sceneRoundTrip.compare(s.sceneRoundTripStr.c_str()) == 0)
		{
			string scene1 = GetWeakSourceName(s.scene1);
			string scene2 = GetWeakSourceName(s.scene2);
			string transitionName = GetWeakSourceName(s.transition);
			int delay = s.delay;
			ui->sceneRoundTripScenes1->setCurrentText(scene1.c_str());
			ui->sceneRoundTripScenes2->setCurrentText(scene2.c_str());
			ui->sceneRoundTripTransitions->setCurrentText(transitionName.c_str());
			ui->sceneRoundTripSpinBox->setValue((double)delay/1000);
			break;
		}
	}
}


int SceneSwitcher::SceneRoundTripFindByData(const QString& scene1)
{
	QRegExp rx(scene1 + " ->.*");
	int count = ui->sceneRoundTrips->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->sceneRoundTrips->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString))
		{
			idx = i;
			break;
		}
	}

	return idx;
}


void SwitcherData::autoStopStreamAndRecording()
{
	obs_source_t* currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t* ws = obs_source_get_weak_source(currentSource);

	if (ws && autoStopScene == ws)
	{
		if (obs_frontend_streaming_active())
			obs_frontend_streaming_stop();
		if (obs_frontend_recording_active())
			obs_frontend_recording_stop();
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

