#include <QFileDialog>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"
#include "headers/version.h"

constexpr auto tab_count = 15;

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
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = NO_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(true);
}

void AdvSceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = SWITCH;
	ui->noMatchSwitchScene->setEnabled(true);
	UpdateNonMatchingScene(ui->noMatchSwitchScene->currentText());
	ui->randomDisabledWarning->setVisible(true);
}

void AdvSceneSwitcher::on_noMatchRandomSwitch_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = RANDOM_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(false);
}

void AdvSceneSwitcher::on_noMatchDelay_valueChanged(double i)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->noMatchDelay = i;
}

void AdvSceneSwitcher::on_startupBehavior_currentIndexChanged(int index)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->startupBehavior = (StartupBehavior)index;
}

void AdvSceneSwitcher::on_autoStartEvent_currentIndexChanged(int index)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->autoStartEvent = static_cast<AutoStartEvent>(index);
}

void AdvSceneSwitcher::on_noMatchSwitchScene_currentTextChanged(
	const QString &text)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateNonMatchingScene(text);
}

void AdvSceneSwitcher::on_cooldownTime_valueChanged(double i)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->cooldown = i;
}

void AdvSceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->interval = value;
}

void AdvSceneSwitcher::SetStarted()
{
	ui->toggleStartButton->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.stop"));
	ui->pluginRunningText->setText(
		obs_module_text("AdvSceneSwitcher.status.active"));
	ui->pluginRunningText->disconnect(inactivePluse);
}

void AdvSceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	ui->pluginRunningText->setText(
		obs_module_text("AdvSceneSwitcher.status.inactive"));
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

void AdvSceneSwitcher::on_verboseLogging_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->verbose = state;
}

void AdvSceneSwitcher::on_uiHintsDisable_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->disableHints = state;
}

void AdvSceneSwitcher::AskBackup(obs_data_t *obj)
{
	bool backupSettings = DisplayMessage(
		obs_module_text("AdvSceneSwitcher.askBackup"), true);

	if (!backupSettings) {
		return;
	}

	QString directory = QFileDialog::getSaveFileName(
		nullptr,
		obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.importWindowTitle"),
		QDir::currentPath(),
		obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType"));
	if (directory.isEmpty()) {
		return;
	}

	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

	obs_data_save_json(obj, file.fileName().toUtf8().constData());
}

void AdvSceneSwitcher::on_exportSettings_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.exportWindowTitle")),
		QDir::currentPath(),
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType")));
	if (directory.isEmpty()) {
		return;
	}

	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

	obs_data_t *obj = obs_data_create();

	switcher->saveSettings(obj);

	obs_data_save_json(obj, file.fileName().toUtf8().constData());

	obs_data_release(obj);
}

void AdvSceneSwitcher::on_importSettings_clicked()
{
	// Scene switcher could be stuck in a sequence
	// so it needs to be stopped before importing new settings
	bool start = !switcher->stop;
	switcher->Stop();

	std::lock_guard<std::mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.importWindowTitle")),
		QDir::currentPath(),
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType")));
	if (directory.isEmpty()) {
		return;
	}

	QFile file(directory);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}

	obs_data_t *obj = obs_data_create_from_json_file(
		file.fileName().toUtf8().constData());

	if (!obj) {
		(void)DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.loadFail"));
		return;
	}

	switcher->loadSettings(obj);

	obs_data_release(obj);

	(void)DisplayMessage(obs_module_text(
		"AdvSceneSwitcher.generalTab.saveOrLoadsettings.loadSuccess"));
	close();

	// Restart scene switcher if it was active
	if (start) {
		switcher->Start();
	}
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
		tabName = "pauseTab";
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
	case 13:
		tabName = "sceneGroupTab";
		break;
	case 14:
		tabName = "sceneTriggerTab";
		break;
	}

	QWidget *page = tabWidget->findChild<QWidget *>(tabName);
	if (page) {
		at = tabWidget->indexOf(page);
	}
	if (at == -1) {
		blog(LOG_INFO, "failed to find tab %s",
		     tabName.toUtf8().constData());
	}

	return at;
}

void AdvSceneSwitcher::setTabOrder()
{
	QTabBar *bar = ui->tabWidget->tabBar();
	for (int i = 0; i < bar->count(); ++i) {
		int curPos = findTabIndex(ui->tabWidget, switcher->tabOrder[i]);

		if (i != curPos && curPos != -1) {
			bar->moveTab(curPos, i);
		}
	}

	connect(bar, &QTabBar::tabMoved, this, &AdvSceneSwitcher::on_tabMoved);
}

void AdvSceneSwitcher::on_tabMoved(int from, int to)
{
	if (loading) {
		return;
	}

	std::swap(switcher->tabOrder[from], switcher->tabOrder[to]);
}

void AdvSceneSwitcher::on_tabWidget_currentChanged(int index)
{
	UNUSED_PARAMETER(index);

	switcher->showFrame = false;
	clearFrames(ui->screenRegionSwitches);
	SetShowFrames();
}

void SwitcherData::loadSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	switcher->loadSceneGroups(obj);
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
	switcher->loadSceneTriggers(obj);
	switcher->loadGeneralSettings(obj);
	switcher->loadHotkeys(obj);
}

void SwitcherData::saveSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	saveSceneGroups(obj);
	saveWindowTitleSwitches(obj);
	saveScreenRegionSwitches(obj);
	savePauseSwitches(obj);
	saveSceneSequenceSwitches(obj);
	saveSceneTransitions(obj);
	saveIdleSwitches(obj);
	saveExecutableSwitches(obj);
	saveRandomSwitches(obj);
	saveFileSwitches(obj);
	saveMediaSwitches(obj);
	saveTimeSwitches(obj);
	saveAudioSwitches(obj);
	saveSceneTriggers(obj);
	saveGeneralSettings(obj);
	saveHotkeys(obj);
	saveVersion(obj, g_GIT_SHA1);
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
	obs_data_set_double(obj, "noMatchDelay", switcher->noMatchDelay);

	obs_data_set_double(obj, "cooldown", switcher->cooldown);

	obs_data_set_bool(obj, "active", !switcher->stop);
	obs_data_set_int(obj, "startup_behavior", switcher->startupBehavior);

	obs_data_set_int(obj, "autoStartEvent",
			 static_cast<int>(switcher->autoStartEvent));

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
	if (switcher->tabOrder.size() < tab_count) {
		switcher->tabOrder = std::vector<int>(tab_count);
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
	obs_data_set_int(obj, "sceneGroupTabPos", switcher->tabOrder[13]);
	obs_data_set_int(obj, "triggerTabPos", switcher->tabOrder[14]);
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
	switcher->noMatchDelay = obs_data_get_double(obj, "noMatchDelay");

	switcher->cooldown = obs_data_get_double(obj, "cooldown");

	switcher->stop = !obs_data_get_bool(obj, "active");
	switcher->startupBehavior =
		(StartupBehavior)obs_data_get_int(obj, "startup_behavior");
	if (switcher->startupBehavior == START) {
		switcher->stop = false;
	}
	if (switcher->startupBehavior == STOP) {
		switcher->stop = true;
	}

	switcher->autoStartEvent = static_cast<AutoStartEvent>(
		obs_data_get_int(obj, "autoStartEvent"));

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
	obs_data_set_default_int(obj, "sceneGroupTabPos", 13);
	obs_data_set_default_int(obj, "triggerTabPos", 14);

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
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "sceneGroupTabPos")));
	switcher->tabOrder.emplace_back(
		(int)(obs_data_get_int(obj, "triggerTabPos")));
}

void SwitcherData::checkNoMatchSwitch(bool &match, OBSWeakSource &scene,
				      OBSWeakSource &transition, int &sleep)
{
	if (match) {
		noMatchCount = 0;
		return;
	}

	if ((noMatchCount * interval) / 1000.0 < noMatchDelay) {
		noMatchCount++;
		return;
	}

	if (switchIfNotMatching == SWITCH && nonMatchingScene) {
		match = true;
		scene = nonMatchingScene;
		transition = nullptr;
	}
	if (switchIfNotMatching == RANDOM_SWITCH) {
		checkRandom(match, scene, transition, sleep);
	}
}

void SwitcherData::checkSwitchCooldown(bool &match)
{
	if (!match || cooldown == 0.) {
		return;
	}

	auto now = std::chrono::high_resolution_clock::now();
	auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
		now - lastMatchTime);

	if (timePassed.count() > cooldown * 1000) {
		lastMatchTime = now;
		return;
	}

	match = false;
	vblog(LOG_INFO, "cooldown active - ignoring match");
}

void populateStartupBehavior(QComboBox *cb)
{
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.asLastRun"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.alwaysStart"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.doNotStart"));
}

void populateAutoStartEventSelection(QComboBox *cb)
{
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.autoStart.never"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.autoStart.recording"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.autoStart.streaming"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.autoStart.recordingAndStreaming"));
}

void AdvSceneSwitcher::setupGeneralTab()
{
	populateSceneSelection(ui->noMatchSwitchScene, false);

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
	ui->noMatchDelay->setValue(switcher->noMatchDelay);
	ui->noMatchDelay->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.onNoMetDelayTooltip"));
	ui->checkInterval->setValue(switcher->interval);

	ui->cooldownTime->setValue(switcher->cooldown);
	ui->cooldownTime->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.cooldownHint"));

	ui->verboseLogging->setChecked(switcher->verbose);
	ui->uiHintsDisable->setChecked(switcher->disableHints);

	for (int p : switcher->functionNamesByPriority) {
		std::string s = "";
		switch (p) {
		case read_file_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.fileContent");
			break;
		case round_trip_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.sceneSequence");
			break;
		case idle_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.idleDetection");
			break;
		case exe_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.executable");
			break;
		case screen_region_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.screenRegion");
			break;
		case window_title_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.windowTitle");
			break;
		case media_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.media");
			break;
		case time_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.time");
			break;
		case audio_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.audio");
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

	populateStartupBehavior(ui->startupBehavior);
	ui->startupBehavior->setCurrentIndex(switcher->startupBehavior);

	populateAutoStartEventSelection(ui->autoStartEvent);
	ui->autoStartEvent->setCurrentIndex(
		static_cast<int>(switcher->autoStartEvent));

	if (switcher->th && switcher->th->isRunning()) {
		SetStarted();
	} else {
		SetStopped();
	}
}
