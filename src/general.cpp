#include <QFileDialog>
#include <QMessageBox>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

QMetaObject::Connection inactivePluse;

void AdvSceneSwitcher::on_close_clicked()
{
	done(0);
}

void AdvSceneSwitcher::UpdateNonMatchingScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->nonMatchingScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void AdvSceneSwitcher::on_noMatchDontSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = NO_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(true);
}

void AdvSceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = SWITCH;
	ui->noMatchSwitchScene->setEnabled(true);
	UpdateNonMatchingScene(ui->noMatchSwitchScene->currentText());
	ui->randomDisabledWarning->setVisible(true);
}

void AdvSceneSwitcher::on_noMatchRandomSwitch_clicked()
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = RANDOM_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(false);
}

void AdvSceneSwitcher::on_startupBehavior_currentIndexChanged(int index)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->startupBehavior = (StartupBehavior)index;
}

void AdvSceneSwitcher::on_noMatchSwitchScene_currentTextChanged(
	const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateNonMatchingScene(text);
}

void AdvSceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->interval = value;
}

void AdvSceneSwitcher::SetStarted()
{
	ui->toggleStartButton->setText("Stop");
	ui->pluginRunningText->setText("Active");
	ui->pluginRunningText->disconnect(inactivePluse);
}

void AdvSceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText("Start");
	ui->pluginRunningText->setText("Inactive");
	inactivePluse = PulseWidget(ui->pluginRunningText, QColor(Qt::red),
				    QColor(0, 0, 0, 0), "QLabel ");
}

void AdvSceneSwitcher::on_toggleStartButton_clicked()
{
	if (switcher->th && switcher->th->isRunning()) {
		switcher->Stop();
		SetStopped();
	} else {
		switcher->Start();
		SetStarted();
	}
}

void AdvSceneSwitcher::closeEvent(QCloseEvent *)
{
	obs_frontend_save();
}

void AdvSceneSwitcher::on_autoStopScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateAutoStopScene(text);
}

void AdvSceneSwitcher::on_autoStopSceneCheckBox_stateChanged(int state)
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
		if (!switcher->autoStopScene)
			UpdateAutoStopScene(ui->autoStopScenes->currentText());
	}
}

void AdvSceneSwitcher::UpdateAutoStopScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->autoStopScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SwitcherData::autoStopStreamAndRecording()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	if (ws && autoStopScene == ws) {
		if (obs_frontend_streaming_active()) {
			blog(LOG_INFO, "Stopping stream because scene '%s' is active", obs_source_get_name(currentSource));
			obs_frontend_streaming_stop();
		}
		if (obs_frontend_recording_active()) {
			blog(LOG_INFO, "Stopping record because scene '%s' is active", obs_source_get_name(currentSource));
			obs_frontend_recording_stop();
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);
}

void AdvSceneSwitcher::on_autoStartType_currentIndexChanged(int index)
{
	if (loading)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->autoStartType = (AutoStartType)index;
}

void AdvSceneSwitcher::on_autoStartScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateAutoStartScene(text);
}

void AdvSceneSwitcher::on_autoStartSceneCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		ui->autoStartScenes->setDisabled(true);
		ui->autoStartType->setDisabled(true);
		switcher->autoStartEnable = false;
	} else {
		ui->autoStartScenes->setDisabled(false);
		ui->autoStartType->setDisabled(false);
		switcher->autoStartEnable = true;
		if (!switcher->autoStartScene)
			UpdateAutoStartScene(
				ui->autoStartScenes->currentText());
	}
}

void AdvSceneSwitcher::UpdateAutoStartScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->autoStartScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SwitcherData::autoStartStreamRecording()
{
	if (autoStartedRecently)
		return;

	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	if (ws && autoStartScene == ws) {
		if ((switcher->autoStartType == STREAMING ||
		     switcher->autoStartType == RECORINDGSTREAMING) &&
		    !obs_frontend_streaming_active()) {
			blog(LOG_INFO, "Starting stream because scene '%s' is active", obs_source_get_name(currentSource));
			obs_frontend_streaming_start();
		}
		if ((switcher->autoStartType == RECORDING ||
		     switcher->autoStartType == RECORINDGSTREAMING) &&
		    !obs_frontend_recording_active()) {
			blog(LOG_INFO, "Starting record because scene '%s' is active", obs_source_get_name(currentSource));
			obs_frontend_recording_start();
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);

	autoStartedRecently = true;
}

void AdvSceneSwitcher::on_verboseLogging_stateChanged(int state)
{
	if (loading)
		return;
	switcher->verbose = state;
}

void AdvSceneSwitcher::on_uiHintsDisable_stateChanged(int state)
{
	if (loading)
		return;
	switcher->disableHints = state;
}

void AdvSceneSwitcher::on_exportSettings_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Export Advanced Scene Switcher settings to file ..."),
		QDir::currentPath(), tr("Text files (*.txt)"));
	if (directory.isEmpty())
		return;

	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	obs_data_t *obj = obs_data_create();

	switcher->saveWindowTitleSwitches(obj);
	switcher->saveScreenRegionSwitches(obj);
	switcher->savePauseSwitches(obj);
	switcher->saveSceneSequenceSwitches(obj);
	switcher->saveSceneTransitions(obj);
	switcher->saveIdleSwitches(obj);
	switcher->saveExecutableSwitches(obj);
	switcher->saveRandomSwitches(obj);
	switcher->saveFileSwitches(obj);
	switcher->saveMediaSwitches(obj);
	switcher->saveTimeSwitches(obj);
	switcher->saveAudioSwitches(obj);
	switcher->saveGeneralSettings(obj);
	switcher->saveHotkeys(obj);

	obs_data_save_json(obj, file.fileName().toUtf8().constData());

	obs_data_release(obj);
}

void AdvSceneSwitcher::on_importSettings_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this,
		tr("Import Advanced Scene Switcher settings from file ..."),
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
			"Advanced Scene Switcher failed to import settings");
		Msgbox.exec();
		return;
	}

	switcher->loadWindowTitleSwitches(obj);
	switcher->loadScreenRegionSwitches(obj);
	switcher->loadPauseSwitches(obj);
	switcher->loadSceneSequenceSwitches(obj);
	switcher->loadSceneTransitions(obj);
	switcher->loadIdleSwitches(obj);
	switcher->loadExecutableSwitches(obj);
	switcher->loadRandomSwitches(obj);
	switcher->loadFileSwitches(obj);
	switcher->loadMediaSwitches(obj);
	switcher->loadTimeSwitches(obj);
	switcher->loadAudioSwitches(obj);
	switcher->loadGeneralSettings(obj);
	switcher->loadHotkeys(obj);

	obs_data_release(obj);

	QMessageBox Msgbox;
	Msgbox.setText(
		"Advanced Scene Switcher settings imported successfully");
	Msgbox.exec();
	close();
}

int findTabIndex(QTabWidget *tabWidget, int pos)
{
	int at = -1;

	QString tabName = "";
	switch (pos) {
	case 0:
		tabName = "generalTab";
		break;
	case 1:
		tabName = "transitionsTab";
		break;
	case 2:
		tabName = "pauseScenesTab";
		break;
	case 3:
		tabName = "windowTitleTab";
		break;
	case 4:
		tabName = "executableTab";
		break;
	case 5:
		tabName = "screenRegionTab";
		break;
	case 6:
		tabName = "mediaTab";
		break;
	case 7:
		tabName = "fileTab";
		break;
	case 8:
		tabName = "randomTab";
		break;
	case 9:
		tabName = "timeTab";
		break;
	case 10:
		tabName = "idleTab";
		break;
	case 11:
		tabName = "sceneSequenceTab";
		break;
	case 12:
		tabName = "audioTab";
		break;
	}

	QWidget *page = tabWidget->findChild<QWidget *>(tabName);
	if (page) {
		at = tabWidget->indexOf(page);
	}
	if (at == -1)
		blog(LOG_INFO, "failed to find tab %s",
		     tabName.toUtf8().constData());

	return at;
}

void AdvSceneSwitcher::setTabOrder()
{
	QTabBar *bar = ui->tabWidget->tabBar();
	for (int i = 0; i < bar->count(); ++i) {
		int curPos = findTabIndex(ui->tabWidget, switcher->tabOrder[i]);

		if (i != curPos && curPos != -1)
			bar->moveTab(curPos, i);
	}

	connect(bar, &QTabBar::tabMoved, this, &AdvSceneSwitcher::on_tabMoved);
}

void AdvSceneSwitcher::on_tabMoved(int from, int to)
{
	if (loading)
		return;
	std::swap(switcher->tabOrder[from], switcher->tabOrder[to]);
}

void SwitcherData::saveGeneralSettings(obs_data_t *obj)
{
	obs_data_set_int(obj, "interval", switcher->interval);

	std::string nonMatchingSceneName =
		GetWeakSourceName(switcher->nonMatchingScene);
	obs_data_set_string(obj, "non_matching_scene",
			    nonMatchingSceneName.c_str());
	obs_data_set_int(obj, "switch_if_not_matching",
			 switcher->switchIfNotMatching);

	obs_data_set_bool(obj, "active", !switcher->stop);
	obs_data_set_int(obj, "startup_behavior", switcher->startupBehavior);

	std::string autoStopSceneName =
		GetWeakSourceName(switcher->autoStopScene);
	obs_data_set_bool(obj, "autoStopEnable", switcher->autoStopEnable);
	obs_data_set_string(obj, "autoStopSceneName",
			    autoStopSceneName.c_str());

	std::string autoStartSceneName =
		GetWeakSourceName(switcher->autoStartScene);
	obs_data_set_bool(obj, "autoStartEnable", switcher->autoStartEnable);
	obs_data_set_int(obj, "autoStartType", switcher->autoStartType);
	obs_data_set_string(obj, "autoStartSceneName",
			    autoStartSceneName.c_str());

	obs_data_set_bool(obj, "verbose", switcher->verbose);
	obs_data_set_bool(obj, "disableHints", switcher->disableHints);

	obs_data_set_int(obj, "priority0",
			 switcher->functionNamesByPriority[0]);
	obs_data_set_int(obj, "priority1",
			 switcher->functionNamesByPriority[1]);
	obs_data_set_int(obj, "priority2",
			 switcher->functionNamesByPriority[2]);
	obs_data_set_int(obj, "priority3",
			 switcher->functionNamesByPriority[3]);
	obs_data_set_int(obj, "priority4",
			 switcher->functionNamesByPriority[4]);
	obs_data_set_int(obj, "priority5",
			 switcher->functionNamesByPriority[5]);
	obs_data_set_int(obj, "priority6",
			 switcher->functionNamesByPriority[6]);
	obs_data_set_int(obj, "priority7",
			 switcher->functionNamesByPriority[7]);
	obs_data_set_int(obj, "priority8",
			 switcher->functionNamesByPriority[8]);

	obs_data_set_int(obj, "threadPriority", switcher->threadPriority);

	// After fresh install of OBS the vector can be empty
	// as save() might be called before first load()
	if (switcher->tabOrder.size() < 13) {
		switcher->tabOrder = std::vector<int>(13);
		std::iota(switcher->tabOrder.begin(), switcher->tabOrder.end(),
			  0);
	}

	obs_data_set_int(obj, "generalTabPos", switcher->tabOrder[0]);
	obs_data_set_int(obj, "transitionTabPos", switcher->tabOrder[1]);
	obs_data_set_int(obj, "pauseTabPos", switcher->tabOrder[2]);
	obs_data_set_int(obj, "titleTabPos", switcher->tabOrder[3]);
	obs_data_set_int(obj, "exeTabPos", switcher->tabOrder[4]);
	obs_data_set_int(obj, "regionTabPos", switcher->tabOrder[5]);
	obs_data_set_int(obj, "mediaTabPos", switcher->tabOrder[6]);
	obs_data_set_int(obj, "fileTabPos", switcher->tabOrder[7]);
	obs_data_set_int(obj, "randomTabPos", switcher->tabOrder[8]);
	obs_data_set_int(obj, "timeTabPos", switcher->tabOrder[9]);
	obs_data_set_int(obj, "idleTabPos", switcher->tabOrder[10]);
	obs_data_set_int(obj, "sequenceTabPos", switcher->tabOrder[11]);
	obs_data_set_int(obj, "audioTabPos", switcher->tabOrder[12]);
}

void SwitcherData::loadGeneralSettings(obs_data_t *obj)
{
	obs_data_set_default_int(obj, "interval", default_interval);
	switcher->interval = obs_data_get_int(obj, "interval");

	obs_data_set_default_int(obj, "switch_if_not_matching", NO_SWITCH);
	switcher->switchIfNotMatching =
		(NoMatch)obs_data_get_int(obj, "switch_if_not_matching");
	std::string nonMatchingScene =
		obs_data_get_string(obj, "non_matching_scene");
	switcher->nonMatchingScene =
		GetWeakSourceByName(nonMatchingScene.c_str());

	switcher->stop = !obs_data_get_bool(obj, "active");
	switcher->startupBehavior =
		(StartupBehavior)obs_data_get_int(obj, "startup_behavior");
	if (switcher->startupBehavior == START)
		switcher->stop = false;
	if (switcher->startupBehavior == STOP)
		switcher->stop = true;

	std::string autoStopScene =
		obs_data_get_string(obj, "autoStopSceneName");
	switcher->autoStopEnable = obs_data_get_bool(obj, "autoStopEnable");
	switcher->autoStopScene = GetWeakSourceByName(autoStopScene.c_str());

	std::string autoStartScene =
		obs_data_get_string(obj, "autoStartSceneName");
	switcher->autoStartEnable = obs_data_get_bool(obj, "autoStartEnable");
	switcher->autoStartType =
		(AutoStartType)obs_data_get_int(obj, "autoStartType");
	switcher->autoStartScene = GetWeakSourceByName(autoStartScene.c_str());

	switcher->verbose = obs_data_get_bool(obj, "verbose");
	switcher->disableHints = obs_data_get_bool(obj, "disableHints");

	obs_data_set_default_int(obj, "priority0", default_priority_0);
	obs_data_set_default_int(obj, "priority1", default_priority_1);
	obs_data_set_default_int(obj, "priority2", default_priority_2);
	obs_data_set_default_int(obj, "priority3", default_priority_3);
	obs_data_set_default_int(obj, "priority4", default_priority_4);
	obs_data_set_default_int(obj, "priority5", default_priority_5);
	obs_data_set_default_int(obj, "priority6", default_priority_6);
	obs_data_set_default_int(obj, "priority7", default_priority_7);
	obs_data_set_default_int(obj, "priority8", default_priority_8);

	switcher->functionNamesByPriority[0] =
		(obs_data_get_int(obj, "priority0"));
	switcher->functionNamesByPriority[1] =
		(obs_data_get_int(obj, "priority1"));
	switcher->functionNamesByPriority[2] =
		(obs_data_get_int(obj, "priority2"));
	switcher->functionNamesByPriority[3] =
		(obs_data_get_int(obj, "priority3"));
	switcher->functionNamesByPriority[4] =
		(obs_data_get_int(obj, "priority4"));
	switcher->functionNamesByPriority[5] =
		(obs_data_get_int(obj, "priority5"));
	switcher->functionNamesByPriority[6] =
		(obs_data_get_int(obj, "priority6"));
	switcher->functionNamesByPriority[7] =
		(obs_data_get_int(obj, "priority7"));
	switcher->functionNamesByPriority[8] =
		(obs_data_get_int(obj, "priority8"));
	if (!switcher->prioFuncsValid()) {
		switcher->functionNamesByPriority[0] = (default_priority_0);
		switcher->functionNamesByPriority[1] = (default_priority_1);
		switcher->functionNamesByPriority[2] = (default_priority_2);
		switcher->functionNamesByPriority[3] = (default_priority_3);
		switcher->functionNamesByPriority[4] = (default_priority_4);
		switcher->functionNamesByPriority[5] = (default_priority_5);
		switcher->functionNamesByPriority[6] = (default_priority_6);
		switcher->functionNamesByPriority[7] = (default_priority_7);
		switcher->functionNamesByPriority[8] = (default_priority_8);
	}

	obs_data_set_default_int(obj, "threadPriority",
				 QThread::NormalPriority);
	switcher->threadPriority = obs_data_get_int(obj, "threadPriority");

	obs_data_set_default_int(obj, "generalTabPos", 0);
	obs_data_set_default_int(obj, "transitionTabPos", 1);
	obs_data_set_default_int(obj, "pauseTabPos", 2);
	obs_data_set_default_int(obj, "titleTabPos", 3);
	obs_data_set_default_int(obj, "exeTabPos", 4);
	obs_data_set_default_int(obj, "regionTabPos", 5);
	obs_data_set_default_int(obj, "mediaTabPos", 6);
	obs_data_set_default_int(obj, "fileTabPos", 7);
	obs_data_set_default_int(obj, "randomTabPos", 8);
	obs_data_set_default_int(obj, "timeTabPos", 9);
	obs_data_set_default_int(obj, "idleTabPos", 10);
	obs_data_set_default_int(obj, "sequenceTabPos", 11);
	obs_data_set_default_int(obj, "audioTabPos", 12);

	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "generalTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "transitionTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "pauseTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "titleTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "exeTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "regionTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "mediaTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "fileTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "randomTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "timeTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "idleTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "sequenceTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "audioTabPos")));
}

void AdvSceneSwitcher::setupGeneralTab()
{
	populateSceneSelection(ui->noMatchSwitchScene, false);
	populateSceneSelection(ui->autoStopScenes, false);
	populateSceneSelection(ui->autoStartScenes, false);

	if (switcher->switchIfNotMatching == SWITCH) {
		ui->noMatchSwitch->setChecked(true);
		ui->noMatchSwitchScene->setEnabled(true);
	} else if (switcher->switchIfNotMatching == NO_SWITCH) {
		ui->noMatchDontSwitch->setChecked(true);
		ui->noMatchSwitchScene->setEnabled(false);
	} else {
		ui->noMatchRandomSwitch->setChecked(true);
		ui->noMatchSwitchScene->setEnabled(false);
	}
	ui->noMatchSwitchScene->setCurrentText(
		GetWeakSourceName(switcher->nonMatchingScene).c_str());
	ui->checkInterval->setValue(switcher->interval);

	ui->autoStopSceneCheckBox->setChecked(switcher->autoStopEnable);
	ui->autoStopScenes->setCurrentText(
		GetWeakSourceName(switcher->autoStopScene).c_str());

	if (ui->autoStopSceneCheckBox->checkState()) {
		ui->autoStopScenes->setDisabled(false);
	} else {
		ui->autoStopScenes->setDisabled(true);
	}

	ui->autoStartType->addItem("Recording");
	ui->autoStartType->addItem("Streaming");
	ui->autoStartType->addItem("Recording and Streaming");

	ui->autoStartSceneCheckBox->setChecked(switcher->autoStartEnable);
	ui->autoStartScenes->setCurrentText(
		GetWeakSourceName(switcher->autoStartScene).c_str());
	ui->autoStartType->setCurrentIndex(switcher->autoStartType);

	if (ui->autoStartSceneCheckBox->checkState()) {
		ui->autoStartScenes->setDisabled(false);
		ui->autoStartType->setDisabled(false);
	} else {
		ui->autoStartScenes->setDisabled(true);
		ui->autoStartType->setDisabled(true);
	}

	ui->verboseLogging->setChecked(switcher->verbose);
	ui->uiHintsDisable->setChecked(switcher->disableHints);

	for (int p : switcher->functionNamesByPriority) {
		std::string s = "";
		switch (p) {
		case read_file_func:
			s = "File Content";
			break;
		case round_trip_func:
			s = "Scene Sequence";
			break;
		case idle_func:
			s = "Idle Detection";
			break;
		case exe_func:
			s = "Executable";
			break;
		case screen_region_func:
			s = "Screen Region";
			break;
		case window_title_func:
			s = "Window Title";
			break;
		case media_func:
			s = "Media";
			break;
		case time_func:
			s = "Time";
			break;
		case audio_func:
			s = "Audio";
			break;
		}
		QString text(s.c_str());
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->priorityList);
		item->setData(Qt::UserRole, text);
	}

	for (int i = 0; i < (int)switcher->threadPriorities.size(); ++i) {
		ui->threadPriority->addItem(
			switcher->threadPriorities[i].name.c_str());
		ui->threadPriority->setItemData(
			i, switcher->threadPriorities[i].description.c_str(),
			Qt::ToolTipRole);
		if (switcher->threadPriority ==
		    switcher->threadPriorities[i].value) {
			ui->threadPriority->setCurrentText(
				switcher->threadPriorities[i].name.c_str());
		}
	}

	ui->startupBehavior->addItem(
		"Start the scene switcher if it was running");
	ui->startupBehavior->addItem("Always start the scene switcher");
	ui->startupBehavior->addItem("Do not start the scene switcher");

	ui->startupBehavior->setCurrentIndex(switcher->startupBehavior);

	if (switcher->th && switcher->th->isRunning())
		SetStarted();
	else
		SetStopped();
}
