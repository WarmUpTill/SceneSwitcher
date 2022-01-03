#include "headers/advanced-scene-switcher.hpp"
#include "headers/status-dock.hpp"
#include "headers/utility.hpp"
#include "headers/version.h"

#include <QFileDialog>
#include <QStandardPaths>

QMetaObject::Connection inactivePluse;

void AdvSceneSwitcher::on_close_clicked()
{
	close();
}

void AdvSceneSwitcher::reject()
{
	close();
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

void AdvSceneSwitcher::NoMatchDelayDurationChanged(double sec)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->noMatchDelay.seconds = sec;
}

void AdvSceneSwitcher::NoMatchDelayUnitChanged(DurationUnit unit)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->noMatchDelay.displayUnit = unit;
}

void AdvSceneSwitcher::CooldownDurationChanged(double sec)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->cooldown.seconds = sec;
}

void AdvSceneSwitcher::CooldownUnitChanged(DurationUnit unit)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->cooldown.displayUnit = unit;
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
	currentStatusActive = true;
}

void AdvSceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	ui->pluginRunningText->setText(
		obs_module_text("AdvSceneSwitcher.status.inactive"));
	if (!switcher->disableHints) {
		inactivePluse = PulseWidget(ui->pluginRunningText,
					    QColor(Qt::red), QColor(0, 0, 0, 0),
					    "QLabel ");
	}
	currentStatusActive = false;
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
	if (!switcher) {
		return;
	}
	switcher->windowPos = this->pos();
	switcher->windowSize = this->size();

	obs_frontend_save();
}

void AdvSceneSwitcher::on_verboseLogging_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->verbose = state;
}

void AdvSceneSwitcher::on_saveWindowGeo_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->saveWindowGeo = state;
}

void AdvSceneSwitcher::on_showTrayNotifications_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->showSystemTrayNotifications = state;
}

void AdvSceneSwitcher::on_uiHintsDisable_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->disableHints = state;
}

QString getDefaultSaveLocation()
{
	QString desktopPath = QStandardPaths::writableLocation(
		QStandardPaths::DesktopLocation);
	auto scName = obs_frontend_get_current_scene_collection();
	QString sceneCollectionName(scName);
	bfree(scName);
	auto timestamp = QDateTime::currentDateTime();
	return desktopPath + "/adv-ss-" + sceneCollectionName + "-" +
	       timestamp.toString("yyyy.MM.dd.hh.mm.ss");
}

void AskForBackup(obs_data_t *obj)
{
	bool backupSettings = DisplayMessage(
		obs_module_text("AdvSceneSwitcher.askBackup"), true);

	if (!backupSettings) {
		return;
	}

	QString path = QFileDialog::getSaveFileName(
		nullptr,
		obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.importWindowTitle"),
		getDefaultSaveLocation(),
		obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType"));
	if (path.isEmpty()) {
		return;
	}

	QFile file(path);
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
		getDefaultSaveLocation(),
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
		tabName = "macroTab";
		break;
	case 2:
		tabName = "transitionsTab";
		break;
	case 3:
		tabName = "pauseTab";
		break;
	case 4:
		tabName = "windowTitleTab";
		break;
	case 5:
		tabName = "executableTab";
		break;
	case 6:
		tabName = "screenRegionTab";
		break;
	case 7:
		tabName = "mediaTab";
		break;
	case 8:
		tabName = "fileTab";
		break;
	case 9:
		tabName = "randomTab";
		break;
	case 10:
		tabName = "timeTab";
		break;
	case 11:
		tabName = "idleTab";
		break;
	case 12:
		tabName = "sceneSequenceTab";
		break;
	case 13:
		tabName = "audioTab";
		break;
	case 14:
		tabName = "videoTab";
		break;
	case 15:
		tabName = "networkTab";
		break;
	case 16:
		tabName = "sceneGroupTab";
		break;
	case 17:
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
	if (!switcher->tabOrderValid()) {
		switcher->resetTabOrder();
	}

	QTabBar *bar = ui->tabWidget->tabBar();
	for (int i = 0; i < bar->count(); ++i) {
		int curPos = findTabIndex(ui->tabWidget, switcher->tabOrder[i]);

		if (i != curPos && curPos != -1) {
			bar->moveTab(curPos, i);
		}
	}

	connect(bar, &QTabBar::tabMoved, this, &AdvSceneSwitcher::on_tabMoved);
}

void AdvSceneSwitcher::restoreWindowGeo()
{
	if (switcher->saveWindowGeo && windowPosValid(switcher->windowPos)) {
		this->resize(switcher->windowSize);
		this->move(switcher->windowPos);
	}
}

void AdvSceneSwitcher::on_tabMoved(int from, int to)
{
	if (loading) {
		return;
	}

	std::swap(switcher->tabOrder[from], switcher->tabOrder[to]);
}

void AdvSceneSwitcher::on_tabWidget_currentChanged(int)
{
	switcher->showFrame = false;
	clearFrames(ui->screenRegionSwitches);
	SetShowFrames();
}

void SwitcherData::loadSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	loadSceneGroups(obj);
	loadMacros(obj);
	loadWindowTitleSwitches(obj);
	loadScreenRegionSwitches(obj);
	loadPauseSwitches(obj);
	loadSceneSequenceSwitches(obj);
	loadSceneTransitions(obj);
	loadIdleSwitches(obj);
	loadExecutableSwitches(obj);
	loadRandomSwitches(obj);
	loadFileSwitches(obj);
	loadMediaSwitches(obj);
	loadTimeSwitches(obj);
	loadAudioSwitches(obj);
	loadVideoSwitches(obj);
	loadNetworkSettings(obj);
	loadSceneTriggers(obj);
	loadGeneralSettings(obj);
	loadHotkeys(obj);
	loadDock(obj);
}

void SwitcherData::saveSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	// Needs to be loaded before any entries which might rely on scene group
	// selections to be available.
	saveSceneGroups(obj);

	saveMacros(obj);
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
	saveVideoSwitches(obj);
	saveNetworkSwitches(obj);
	saveSceneTriggers(obj);
	saveGeneralSettings(obj);
	saveHotkeys(obj);
	saveDock(obj);
	saveVersion(obj, g_GIT_SHA1);
}

void SwitcherData::saveGeneralSettings(obs_data_t *obj)
{
	obs_data_set_int(obj, "interval", interval);

	std::string nonMatchingSceneName = GetWeakSourceName(nonMatchingScene);
	obs_data_set_string(obj, "non_matching_scene",
			    nonMatchingSceneName.c_str());
	obs_data_set_int(obj, "switch_if_not_matching", switchIfNotMatching);
	noMatchDelay.Save(obj, "noMatchDelay", "noMatchDelayUnit");

	cooldown.Save(obj, "cooldown", "cooldownUnit");

	obs_data_set_bool(obj, "active", !stop);
	obs_data_set_int(obj, "startup_behavior", startupBehavior);

	obs_data_set_int(obj, "autoStartEvent",
			 static_cast<int>(autoStartEvent));

	obs_data_set_bool(obj, "verbose", verbose);
	obs_data_set_bool(obj, "showSystemTrayNotifications",
			  showSystemTrayNotifications);
	obs_data_set_bool(obj, "disableHints", disableHints);

	obs_data_set_int(obj, "priority0", functionNamesByPriority[0]);
	obs_data_set_int(obj, "priority1", functionNamesByPriority[1]);
	obs_data_set_int(obj, "priority2", functionNamesByPriority[2]);
	obs_data_set_int(obj, "priority3", functionNamesByPriority[3]);
	obs_data_set_int(obj, "priority4", functionNamesByPriority[4]);
	obs_data_set_int(obj, "priority5", functionNamesByPriority[5]);
	obs_data_set_int(obj, "priority6", functionNamesByPriority[6]);
	obs_data_set_int(obj, "priority7", functionNamesByPriority[7]);
	obs_data_set_int(obj, "priority8", functionNamesByPriority[8]);
	obs_data_set_int(obj, "priority9", functionNamesByPriority[9]);
	obs_data_set_int(obj, "priority10", functionNamesByPriority[10]);

	obs_data_set_int(obj, "threadPriority", threadPriority);

	obs_data_set_int(obj, "generalTabPos", tabOrder[0]);
	obs_data_set_int(obj, "macroTabPos", tabOrder[1]);
	obs_data_set_int(obj, "transitionTabPos", tabOrder[2]);
	obs_data_set_int(obj, "pauseTabPos", tabOrder[3]);
	obs_data_set_int(obj, "titleTabPos", tabOrder[4]);
	obs_data_set_int(obj, "exeTabPos", tabOrder[5]);
	obs_data_set_int(obj, "regionTabPos", tabOrder[6]);
	obs_data_set_int(obj, "mediaTabPos", tabOrder[7]);
	obs_data_set_int(obj, "fileTabPos", tabOrder[8]);
	obs_data_set_int(obj, "randomTabPos", tabOrder[9]);
	obs_data_set_int(obj, "timeTabPos", tabOrder[10]);
	obs_data_set_int(obj, "idleTabPos", tabOrder[11]);
	obs_data_set_int(obj, "sequenceTabPos", tabOrder[12]);
	obs_data_set_int(obj, "audioTabPos", tabOrder[13]);
	obs_data_set_int(obj, "videoTabPos", tabOrder[14]);
	obs_data_set_int(obj, "networkTabPos", tabOrder[15]);
	obs_data_set_int(obj, "sceneGroupTabPos", tabOrder[16]);
	obs_data_set_int(obj, "triggerTabPos", tabOrder[17]);

	obs_data_set_bool(obj, "saveWindowGeo", saveWindowGeo);
	obs_data_set_int(obj, "windowPosX", windowPos.x());
	obs_data_set_int(obj, "windowPosY", windowPos.y());
	obs_data_set_int(obj, "windowWidth", windowSize.width());
	obs_data_set_int(obj, "windowHeight", windowSize.height());
}

void SwitcherData::loadGeneralSettings(obs_data_t *obj)
{
	obs_data_set_default_int(obj, "interval", default_interval);
	interval = obs_data_get_int(obj, "interval");

	obs_data_set_default_int(obj, "switch_if_not_matching", NO_SWITCH);
	switchIfNotMatching =
		(NoMatch)obs_data_get_int(obj, "switch_if_not_matching");
	std::string nonMatchingSceneName =
		obs_data_get_string(obj, "non_matching_scene");
	nonMatchingScene = GetWeakSourceByName(nonMatchingSceneName.c_str());
	noMatchDelay.Load(obj, "noMatchDelay", "noMatchDelayUnit");

	cooldown.Load(obj, "cooldown", "cooldownUnit");

	stop = !obs_data_get_bool(obj, "active");
	startupBehavior =
		(StartupBehavior)obs_data_get_int(obj, "startup_behavior");
	if (startupBehavior == START) {
		stop = false;
	}
	if (startupBehavior == STOP) {
		stop = true;
	}

	autoStartEvent = static_cast<AutoStartEvent>(
		obs_data_get_int(obj, "autoStartEvent"));

	verbose = obs_data_get_bool(obj, "verbose");
	showSystemTrayNotifications =
		obs_data_get_bool(obj, "showSystemTrayNotifications");
	disableHints = obs_data_get_bool(obj, "disableHints");

	obs_data_set_default_int(obj, "priority0", default_priority_0);
	obs_data_set_default_int(obj, "priority1", default_priority_1);
	obs_data_set_default_int(obj, "priority2", default_priority_2);
	obs_data_set_default_int(obj, "priority3", default_priority_3);
	obs_data_set_default_int(obj, "priority4", default_priority_4);
	obs_data_set_default_int(obj, "priority5", default_priority_5);
	obs_data_set_default_int(obj, "priority6", default_priority_6);
	obs_data_set_default_int(obj, "priority7", default_priority_7);
	obs_data_set_default_int(obj, "priority8", default_priority_8);

	functionNamesByPriority[0] = (obs_data_get_int(obj, "priority0"));
	functionNamesByPriority[1] = (obs_data_get_int(obj, "priority1"));
	functionNamesByPriority[2] = (obs_data_get_int(obj, "priority2"));
	functionNamesByPriority[3] = (obs_data_get_int(obj, "priority3"));
	functionNamesByPriority[4] = (obs_data_get_int(obj, "priority4"));
	functionNamesByPriority[5] = (obs_data_get_int(obj, "priority5"));
	functionNamesByPriority[6] = (obs_data_get_int(obj, "priority6"));
	functionNamesByPriority[7] = (obs_data_get_int(obj, "priority7"));
	functionNamesByPriority[8] = (obs_data_get_int(obj, "priority8"));
	functionNamesByPriority[9] = (obs_data_get_int(obj, "priority9"));
	functionNamesByPriority[10] = (obs_data_get_int(obj, "priority10"));
	if (!prioFuncsValid()) {
		functionNamesByPriority[0] = (default_priority_0);
		functionNamesByPriority[1] = (default_priority_1);
		functionNamesByPriority[2] = (default_priority_2);
		functionNamesByPriority[3] = (default_priority_3);
		functionNamesByPriority[4] = (default_priority_4);
		functionNamesByPriority[5] = (default_priority_5);
		functionNamesByPriority[6] = (default_priority_6);
		functionNamesByPriority[7] = (default_priority_7);
		functionNamesByPriority[8] = (default_priority_8);
		functionNamesByPriority[9] = (default_priority_9);
		functionNamesByPriority[10] = (default_priority_10);
	}

	obs_data_set_default_int(obj, "threadPriority",
				 QThread::NormalPriority);
	threadPriority = obs_data_get_int(obj, "threadPriority");

	obs_data_set_default_int(obj, "generalTabPos", 0);
	obs_data_set_default_int(obj, "macroTabPos", 1);
	obs_data_set_default_int(obj, "transitionTabPos", 2);
	obs_data_set_default_int(obj, "pauseTabPos", 3);
	obs_data_set_default_int(obj, "titleTabPos", 4);
	obs_data_set_default_int(obj, "exeTabPos", 5);
	obs_data_set_default_int(obj, "regionTabPos", 6);
	obs_data_set_default_int(obj, "mediaTabPos", 7);
	obs_data_set_default_int(obj, "fileTabPos", 8);
	obs_data_set_default_int(obj, "randomTabPos", 9);
	obs_data_set_default_int(obj, "timeTabPos", 10);
	obs_data_set_default_int(obj, "idleTabPos", 11);
	obs_data_set_default_int(obj, "sequenceTabPos", 12);
	obs_data_set_default_int(obj, "audioTabPos", 13);
	obs_data_set_default_int(obj, "videoTabPos", 14);
	obs_data_set_default_int(obj, "networkTabPos", 15);
	obs_data_set_default_int(obj, "sceneGroupTabPos", 16);
	obs_data_set_default_int(obj, "triggerTabPos", 17);

	tabOrder.clear();
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "generalTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "macroTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "transitionTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "pauseTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "titleTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "exeTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "regionTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "mediaTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "fileTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "randomTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "timeTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "idleTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "sequenceTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "audioTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "videoTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "networkTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "sceneGroupTabPos")));
	tabOrder.emplace_back((int)(obs_data_get_int(obj, "triggerTabPos")));

	if (!tabOrderValid()) {
		resetTabOrder();
	}

	saveWindowGeo = obs_data_get_bool(obj, "saveWindowGeo");
	windowPos = {(int)obs_data_get_int(obj, "windowPosX"),
		     (int)obs_data_get_int(obj, "windowPosY")};
	windowSize = {(int)obs_data_get_int(obj, "windowWidth"),
		      (int)obs_data_get_int(obj, "windowHeight")};
}

bool SwitcherData::tabOrderValid()
{
	auto tmp = std::vector<int>(tab_count);
	std::iota(tmp.begin(), tmp.end(), 0);

	for (auto &p : tmp) {
		auto it = std::find(tabOrder.begin(), tabOrder.end(), p);
		if (it == tabOrder.end()) {
			return false;
		}
	}
	return true;
}

void SwitcherData::resetTabOrder()
{
	tabOrder = std::vector<int>(tab_count);
	std::iota(tabOrder.begin(), tabOrder.end(), 0);
}

void SwitcherData::checkNoMatchSwitch(bool &match, OBSWeakSource &scene,
				      OBSWeakSource &transition, int &sleep)
{
	if (match) {
		noMatchDelay.Reset();
		return;
	}

	if (!noMatchDelay.DurationReached()) {
		return;
	}

	if (switchIfNotMatching == SWITCH && nonMatchingScene) {
		match = true;
		scene = nonMatchingScene;
		transition = nullptr;
	}
	if (switchIfNotMatching == RANDOM_SWITCH) {
		match = checkRandom(scene, transition, sleep);
	}
}

void SwitcherData::checkSwitchCooldown(bool &match)
{
	if (!match) {
		return;
	}

	if (cooldown.DurationReached()) {
		cooldown.Reset();
	} else {
		match = false;
		vblog(LOG_INFO, "cooldown active - ignoring match");
	}
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

void AdvSceneSwitcher::updateStatus()
{
	if (switcher->th && switcher->th->isRunning()) {
		if (currentStatusActive) {
			return;
		}
		SetStarted();
	} else {
		if (!currentStatusActive) {
			return;
		}
		SetStopped();
	}
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

	DurationSelection *noMatchDelay = new DurationSelection();
	noMatchDelay->SetDuration(switcher->noMatchDelay);
	noMatchDelay->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.onNoMetDelayTooltip"));
	ui->noMatchLayout->addWidget(noMatchDelay);
	QWidget::connect(noMatchDelay, SIGNAL(DurationChanged(double)), this,
			 SLOT(NoMatchDelayDurationChanged(double)));
	QWidget::connect(noMatchDelay, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(NoMatchDelayUnitChanged(DurationUnit)));

	ui->checkInterval->setValue(switcher->interval);

	DurationSelection *cooldownTime = new DurationSelection();
	cooldownTime->SetDuration(switcher->cooldown);
	cooldownTime->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.cooldownHint"));
	ui->cooldownLayout->addWidget(cooldownTime);
	ui->cooldownLayout->addStretch();
	QWidget::connect(cooldownTime, SIGNAL(DurationChanged(double)), this,
			 SLOT(CooldownDurationChanged(double)));
	QWidget::connect(cooldownTime, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(CooldownUnitChanged(DurationUnit)));

	ui->verboseLogging->setChecked(switcher->verbose);
	ui->saveWindowGeo->setChecked(switcher->saveWindowGeo);
	ui->showTrayNotifications->setChecked(
		switcher->showSystemTrayNotifications);
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
		case video_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.video");
			break;
		case macro_func:
			s = obs_module_text(
				"AdvSceneSwitcher.generalTab.priority.macro");
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

	// Updates the UI status element if the status changed externally
	// (e.g. via hotkeys)
	QTimer *statusTimer = new QTimer(this);
	connect(statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));
	statusTimer->start(1000);

	setWindowTitle(windowTitle() + " - " + g_GIT_TAG);
}
