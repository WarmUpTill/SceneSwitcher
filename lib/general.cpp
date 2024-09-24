#include "advanced-scene-switcher.hpp"
#include "file-selection.hpp"
#include "filter-combo-box.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "path-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "splitter-helpers.hpp"
#include "status-control.hpp"
#include "switcher-data.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "variable.hpp"
#include "version.h"

#include <obs-frontend-api.h>
#include <QFileDialog>

namespace advss {

void AdvSceneSwitcher::reject()
{
	close();
}

void AdvSceneSwitcher::UpdateNonMatchingScene(const QString &name)
{
	OBSSourceAutoRelease scene =
		obs_get_source_by_name(name.toUtf8().constData());
	OBSWeakSourceAutoRelease ws = obs_source_get_weak_source(scene);

	switcher->nonMatchingScene = ws;
}

void AdvSceneSwitcher::on_noMatchDontSwitch_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = NoMatchBehavior::NO_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(true);
}

void AdvSceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->switchIfNotMatching = NoMatchBehavior::SWITCH;
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
	switcher->switchIfNotMatching = NoMatchBehavior::RANDOM_SWITCH;
	ui->noMatchSwitchScene->setEnabled(false);
	ui->randomDisabledWarning->setVisible(false);
}

void AdvSceneSwitcher::NoMatchDelayDurationChanged(const Duration &dur)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->noMatchDelay = dur;
}

void AdvSceneSwitcher::CooldownDurationChanged(const Duration &dur)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->cooldown = dur;
}

void AdvSceneSwitcher::on_enableCooldown_stateChanged(int state)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->enableCooldown = state;
	ui->cooldownTime->setEnabled(state);
}

void AdvSceneSwitcher::on_startupBehavior_currentIndexChanged(int index)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->startupBehavior =
		static_cast<SwitcherData::StartupBehavior>(index);
}

void AdvSceneSwitcher::on_logLevel_currentIndexChanged(int value)
{
	if (loading) {
		return;
	}
	switcher->logLevel = static_cast<SwitcherData::LogLevel>(value);
}

void AdvSceneSwitcher::on_autoStartEvent_currentIndexChanged(int index)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->autoStartEvent = static_cast<SwitcherData::AutoStart>(index);
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

void AdvSceneSwitcher::closeEvent(QCloseEvent *)
{
	if (!switcher) {
		return;
	}
	switcher->windowPos = this->pos();
	switcher->windowSize = this->size();
	switcher->macroListMacroEditSplitterPosition =
		ui->macroListMacroEditSplitter->sizes();
	MacroSelectionAboutToChange(); // Trigger saving of splitter states

	obs_frontend_save();
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

void AdvSceneSwitcher::on_disableComboBoxFilter_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->disableFilterComboboxFilter = state;
	FilterComboBox::SetFilterBehaviourEnabled(!state);
}

void AdvSceneSwitcher::on_warnPluginLoadFailure_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->warnPluginLoadFailure = state;
}

static bool isLegacyTab(const QString &name)
{
	return name == obs_module_text(
			       "AdvSceneSwitcher.sceneGroupTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.transitionTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.windowTitleTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.executableTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.screenRegionTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.mediaTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.fileTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.randomTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.timeTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.idleTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.sceneSequenceTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.audioTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.videoTab.title") ||
	       name == obs_module_text("AdvSceneSwitcher.pauseTab.title") ||
	       name == obs_module_text(
			       "AdvSceneSwitcher.sceneTriggerTab.title");
}

void AdvSceneSwitcher::on_hideLegacyTabs_stateChanged(int state)
{
	switcher->hideLegacyTabs = state;

	for (int idx = 0; idx < ui->tabWidget->count(); idx++) {
		if (isLegacyTab(ui->tabWidget->tabText(idx))) {
			ui->tabWidget->setTabVisible(idx, !state);
		}
	}
}

void AdvSceneSwitcher::SetDeprecationWarnings()
{
	QString toolTip =
		switcher->disableHints
			? ""
			: obs_module_text(
				  "AdvSceneSwitcher.deprecatedTabWarning");
	for (int idx = 0; idx < ui->tabWidget->count(); idx++) {
		if (isLegacyTab(ui->tabWidget->tabText(idx))) {
			ui->tabWidget->widget(idx)->setToolTip(toolTip);
		}
	}
}

static bool containsSensitiveData(obs_data_t *data)
{
	// Only checking for Twitch tokens and websocket connections for now
	// Might have to be expanded upon / generalized in the future
	OBSDataArrayAutoRelease twitchTokens =
		obs_data_get_array(data, "twitchConnections");
	OBSDataArrayAutoRelease websocketConnections =
		obs_data_get_array(data, "websocketConnections");

	auto isNotEmpty = [](obs_data_array *array) {
		return obs_data_array_count(array) > 0;
	};

	return isNotEmpty(twitchTokens) || isNotEmpty(websocketConnections);
}

void AdvSceneSwitcher::on_exportSettings_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.exportWindowTitle")),
		GetDefaultSettingsSaveLocation(),
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType")));
	if (directory.isEmpty()) {
		return;
	}

	QFile file(directory);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	switcher->SaveSettings(data);
	obs_data_save_json(data, file.fileName().toUtf8().constData());
	if (containsSensitiveData(data)) {
		(void)DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.exportSensitiveDataWarning"));
	}
}

void AdvSceneSwitcher::on_importSettings_clicked()
{
	// Scene switcher could be stuck in a sequence
	// so it needs to be stopped before importing new settings
	bool start = !switcher->stop;
	switcher->Stop();

	auto basePath = FileSelection::ValidPathOrDesktop(
		QString::fromStdString(switcher->lastImportPath));
	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.importWindowTitle")),
		basePath,
		tr(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.textType")));
	if (path.isEmpty()) {
		return;
	}

	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return;
	}

	OBSDataAutoRelease obj = obs_data_create_from_json_file(
		file.fileName().toUtf8().constData());

	if (!obj) {
		(void)DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.generalTab.saveOrLoadsettings.loadFail"));
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->LoadSettings(obj);
	switcher->lastImportPath = path.toStdString();

	(void)DisplayMessage(obs_module_text(
		"AdvSceneSwitcher.generalTab.saveOrLoadsettings.loadSuccess"));
	// Just close the UI and let the user reopen it to not have to
	// implement updating each setting
	close();

	// Restart scene switcher if it was active
	if (start) {
		switcher->Start();
	}
}

static bool windowPosValid(QPoint pos)
{
	return !!QGuiApplication::screenAt(pos);
}

void AdvSceneSwitcher::RestoreWindowGeo()
{
	if (switcher->saveWindowGeo && windowPosValid(switcher->windowPos)) {
		this->resize(switcher->windowSize);
		this->move(switcher->windowPos);
	}
}

void AdvSceneSwitcher::CheckFirstTimeSetup()
{
	if (switcher->firstBoot && !switcher->disableHints) {
		switcher->firstBoot = false;
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.firstBootMessage"));
	}
}

void AdvSceneSwitcher::on_tabWidget_currentChanged(int)
{
	if (loading) {
		return;
	}

	switcher->showFrame = false;
	ClearFrames(ui->screenRegionSwitches);
	SetShowFrames();
}

void AdvSceneSwitcher::on_transitionOverridecheckBox_stateChanged(int state)
{
	if (loading) {
		return;
	}

	if (!state && !switcher->adjustActiveTransitionType) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.generalTab.transitionBehaviorSelectionError"));
		ui->adjustActiveTransitionType->setChecked(true);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->transitionOverrideOverride = state;
}

void AdvSceneSwitcher::on_adjustActiveTransitionType_stateChanged(int state)
{
	if (loading) {
		return;
	}

	// This option only makes sense if we are allowed to use transition overrides
	if (!state && !switcher->transitionOverrideOverride) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.generalTab.transitionBehaviorSelectionError"));
		ui->transitionOverridecheckBox->setChecked(true);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->adjustActiveTransitionType = state;
}

void SwitcherData::LoadSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	// New post load steps to be declared during load
	postLoadSteps.clear();

	// Needs to be loaded before any entries which might rely on scene group
	// selections to be available.
	loadSceneGroups(obj);
	LoadVariables(obj);

	for (const auto &func : loadSteps) {
		func(obj);
	}

	LoadMacros(obj);
	LoadGlobalMacroSettings(obj);
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
	loadSceneTriggers(obj);
	LoadGeneralSettings(obj);
	LoadHotkeys(obj);
	LoadUISettings(obj);

	RunPostLoadSteps();

	// Reset on startup and scene collection change
	ResetLastOpenedTab();
	startupLoadDone = true;
}

void SwitcherData::SaveSettings(obs_data_t *obj)
{
	if (!obj) {
		return;
	}

	saveSceneGroups(obj);
	SaveMacros(obj);
	SaveGlobalMacroSettings(obj);
	SaveVariables(obj);
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
	saveSceneTriggers(obj);
	SaveGeneralSettings(obj);
	SaveHotkeys(obj);
	SaveUISettings(obj);
	SaveVersion(obj, g_GIT_SHA1);

	for (const auto &func : saveSteps) {
		func(obj);
	}
}

void SwitcherData::SaveGeneralSettings(obs_data_t *obj)
{
	obs_data_set_int(obj, "interval", interval);

	std::string nonMatchingSceneName = GetWeakSourceName(nonMatchingScene);
	obs_data_set_string(obj, "non_matching_scene",
			    nonMatchingSceneName.c_str());
	obs_data_set_int(obj, "switch_if_not_matching",
			 static_cast<int>(switchIfNotMatching));
	noMatchDelay.Save(obj, "noMatchDelay");

	cooldown.Save(obj, "cooldown");
	obs_data_set_bool(obj, "enableCooldown", enableCooldown);

	obs_data_set_bool(obj, "active", sceneColletionStop ? true : !stop);
	sceneColletionStop = false;
	obs_data_set_int(obj, "startup_behavior",
			 static_cast<int>(startupBehavior));

	obs_data_set_int(obj, "autoStartEvent",
			 static_cast<int>(autoStartEvent));

	obs_data_set_int(obj, "logLevel", static_cast<int>(logLevel));
	obs_data_set_int(obj, "logLevelVersion", 1);
	obs_data_set_bool(obj, "showSystemTrayNotifications",
			  showSystemTrayNotifications);
	obs_data_set_bool(obj, "disableHints", disableHints);
	obs_data_set_bool(obj, "disableFilterComboboxFilter",
			  disableFilterComboboxFilter);
	obs_data_set_bool(obj, "warnPluginLoadFailure", warnPluginLoadFailure);
	obs_data_set_bool(obj, "hideLegacyTabs", hideLegacyTabs);

	SaveFunctionPriorities(obj, functionNamesByPriority);

	obs_data_set_int(obj, "threadPriority", threadPriority);

	obs_data_set_bool(obj, "transitionOverrideOverride",
			  transitionOverrideOverride);
	obs_data_set_bool(obj, "adjustActiveTransitionType",
			  adjustActiveTransitionType);

	obs_data_set_string(obj, "lastImportPath", lastImportPath.c_str());
}

void SwitcherData::LoadGeneralSettings(obs_data_t *obj)
{
	obs_data_set_default_int(obj, "interval", default_interval);
	interval = obs_data_get_int(obj, "interval");

	obs_data_set_default_int(obj, "switch_if_not_matching",
				 static_cast<int>(NoMatchBehavior::NO_SWITCH));
	switchIfNotMatching = static_cast<NoMatchBehavior>(
		obs_data_get_int(obj, "switch_if_not_matching"));
	std::string nonMatchingSceneName =
		obs_data_get_string(obj, "non_matching_scene");
	nonMatchingScene = GetWeakSourceByName(nonMatchingSceneName.c_str());
	noMatchDelay.Load(obj, "noMatchDelay");

	cooldown.Load(obj, "cooldown");
	if (!obs_data_has_user_value(obj, "enableCooldown")) {
		enableCooldown = cooldown.Seconds() != 0;
	} else {
		enableCooldown = obs_data_get_bool(obj, "enableCooldown");
	}

	obs_data_set_default_bool(obj, "active", true);
	stop = !obs_data_get_bool(obj, "active");
	startupBehavior =
		(StartupBehavior)obs_data_get_int(obj, "startup_behavior");
	if (startupBehavior == StartupBehavior::START) {
		stop = false;
	}
	if (startupBehavior == StartupBehavior::STOP) {
		stop = true;
	}

	autoStartEvent =
		static_cast<AutoStart>(obs_data_get_int(obj, "autoStartEvent"));

	logLevel = static_cast<LogLevel>(obs_data_get_int(obj, "logLevel"));
	if (obs_data_get_int(obj, "logLevelVersion") < 1) {
		enum OldLogLevel { DEFAULT, LOG_ACTION, VERBOSE };
		OldLogLevel oldLogLevel = static_cast<OldLogLevel>(
			obs_data_get_int(obj, "logLevel"));
		switch (oldLogLevel) {
		case DEFAULT:
			logLevel = LogLevel::DEFAULT;
			break;
		case LOG_ACTION:
			logLevel = LogLevel::LOG_ACTION;
			break;
		case VERBOSE:
			logLevel = LogLevel::VERBOSE;
			break;
		default:
			break;
		}
	}

	showSystemTrayNotifications =
		obs_data_get_bool(obj, "showSystemTrayNotifications");
	disableHints = obs_data_get_bool(obj, "disableHints");
	disableFilterComboboxFilter =
		obs_data_get_bool(obj, "disableFilterComboboxFilter");
	obs_data_set_default_bool(obj, "warnPluginLoadFailure", true);
	warnPluginLoadFailure = obs_data_get_bool(obj, "warnPluginLoadFailure");
	obs_data_set_default_bool(obj, "hideLegacyTabs", true);
	hideLegacyTabs = obs_data_get_bool(obj, "hideLegacyTabs");

	SetDefaultFunctionPriorities(obj);
	LoadFunctionPriorities(obj, functionNamesByPriority);
	if (!PrioFuncsValid()) {
		functionNamesByPriority = GetDefaultFunctionPriorityList();
	}

	obs_data_set_default_int(obj, "threadPriority",
				 QThread::NormalPriority);
	threadPriority = obs_data_get_int(obj, "threadPriority");

	transitionOverrideOverride =
		obs_data_get_bool(obj, "transitionOverrideOverride");
	adjustActiveTransitionType =
		obs_data_get_bool(obj, "adjustActiveTransitionType");

	if (!transitionOverrideOverride && !adjustActiveTransitionType) {
		blog(LOG_INFO,
		     "reset transition behaviour to adjust active transition type");
		adjustActiveTransitionType = true;
	}

	lastImportPath = obs_data_get_string(obj, "lastImportPath");
}

void SwitcherData::SaveUISettings(obs_data_t *obj)
{
	SaveTabOrder(obj);

	obs_data_set_bool(obj, "saveWindowGeo", saveWindowGeo);
	obs_data_set_int(obj, "windowPosX", windowPos.x());
	obs_data_set_int(obj, "windowPosY", windowPos.y());
	obs_data_set_int(obj, "windowWidth", windowSize.width());
	obs_data_set_int(obj, "windowHeight", windowSize.height());

	SaveSplitterPos(macroListMacroEditSplitterPosition, obj,
			"macroListMacroEditSplitterPosition");
}

void SwitcherData::LoadUISettings(obs_data_t *obj)
{
	LoadTabOrder(obj);

	saveWindowGeo = obs_data_get_bool(obj, "saveWindowGeo");
	windowPos = {(int)obs_data_get_int(obj, "windowPosX"),
		     (int)obs_data_get_int(obj, "windowPosY")};
	windowSize = {(int)obs_data_get_int(obj, "windowWidth"),
		      (int)obs_data_get_int(obj, "windowHeight")};

	LoadSplitterPos(macroListMacroEditSplitterPosition, obj,
			"macroListMacroEditSplitterPosition");
}

void SwitcherData::CheckNoMatchSwitch(bool &match, OBSWeakSource &scene,
				      OBSWeakSource &transition, int &sleep)
{
	if (match) {
		noMatchDelay.Reset();
		return;
	}

	if (!noMatchDelay.DurationReached()) {
		return;
	}

	if (switchIfNotMatching == NoMatchBehavior::SWITCH &&
	    nonMatchingScene) {
		match = true;
		scene = nonMatchingScene;
		transition = nullptr;
	}
	if (switchIfNotMatching == NoMatchBehavior::RANDOM_SWITCH) {
		match = checkRandom(scene, transition, sleep);
	}
}

void SwitcherData::checkSwitchCooldown(bool &match)
{
	if (!match || !enableCooldown) {
		return;
	}

	if (cooldown.DurationReached()) {
		cooldown.Reset();
	} else {
		match = false;
		vblog(LOG_INFO, "cooldown active - ignoring match");
	}
}

static void populateStartupBehavior(QComboBox *cb)
{
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.asLastRun"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.alwaysStart"));
	cb->addItem(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.onStartup.doNotStart"));
}

static void populateAutoStartEventSelection(QComboBox *cb)
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

static void populatePriorityFunctionList(QListWidget *list)
{
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
		QListWidgetItem *item = new QListWidgetItem(text, list);
		item->setData(Qt::UserRole, text);
	}
}

static void populateThreadPriorityList(QComboBox *list)
{
	for (int i = 0; i < (int)switcher->threadPriorities.size(); ++i) {
		list->addItem(switcher->threadPriorities[i].name.c_str());
		list->setItemData(
			i, switcher->threadPriorities[i].description.c_str(),
			Qt::ToolTipRole);
		if (switcher->threadPriority ==
		    switcher->threadPriorities[i].value) {
			list->setCurrentText(
				switcher->threadPriorities[i].name.c_str());
		}
	}
}

static bool isGeneralTab(const QString &name)
{
	return name == obs_module_text("AdvSceneSwitcher.generalTab.title");
}

static int getGeneralTabIdx(QTabWidget *tabWidget)
{
	for (int idx = 0; idx < tabWidget->count(); idx++) {
		if (isGeneralTab(tabWidget->tabText(idx))) {
			return idx;
		}
	}
	return -1;
}

static void setupGeneralTabInactiveWarning(QTabWidget *tabs)
{
	auto callback = [tabs]() {
		const bool isRunning = switcher && switcher->th &&
				       switcher->th->isRunning();
		auto idx = getGeneralTabIdx(tabs);
		tabs->setTabIcon(
			idx,
			isRunning ? QIcon()
				  : QIcon::fromTheme(
					    "obs",
					    QIcon(":/res/images/warning.svg")));
	};

	auto inactiveTimer = new QTimer(tabs);
	inactiveTimer->setInterval(1000);
	QObject::connect(inactiveTimer, &QTimer::timeout, callback);
	inactiveTimer->start();
}

void AdvSceneSwitcher::SetupGeneralTab()
{
	PopulateSceneSelection(ui->noMatchSwitchScene, false);

	if (switcher->switchIfNotMatching == NoMatchBehavior::SWITCH) {
		ui->noMatchSwitch->setChecked(true);
		ui->noMatchSwitchScene->setEnabled(true);
	} else if (switcher->switchIfNotMatching ==
		   NoMatchBehavior::NO_SWITCH) {
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
	QWidget::connect(noMatchDelay,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(NoMatchDelayDurationChanged(const Duration &)));

	ui->checkInterval->setValue(switcher->interval);

	ui->enableCooldown->setChecked(switcher->enableCooldown);
	ui->cooldownTime->setEnabled(switcher->enableCooldown);
	ui->cooldownTime->SetDuration(switcher->cooldown);
	ui->cooldownTime->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.cooldownHint"));
	QWidget::connect(ui->cooldownTime,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(CooldownDurationChanged(const Duration &)));

	ui->logLevel->setCurrentIndex(static_cast<int>(switcher->logLevel));

	ui->saveWindowGeo->setChecked(switcher->saveWindowGeo);
	ui->showTrayNotifications->setChecked(
		switcher->showSystemTrayNotifications);
	ui->uiHintsDisable->setChecked(switcher->disableHints);
	ui->disableComboBoxFilter->setChecked(
		switcher->disableFilterComboboxFilter);
	FilterComboBox::SetFilterBehaviourEnabled(
		!switcher->disableFilterComboboxFilter);
	ui->warnPluginLoadFailure->setChecked(switcher->warnPluginLoadFailure);
	ui->hideLegacyTabs->setChecked(switcher->hideLegacyTabs);

	populatePriorityFunctionList(ui->priorityList);
	populateThreadPriorityList(ui->threadPriority);

	populateStartupBehavior(ui->startupBehavior);
	ui->startupBehavior->setCurrentIndex(
		static_cast<int>(switcher->startupBehavior));

	populateAutoStartEventSelection(ui->autoStartEvent);
	ui->autoStartEvent->setCurrentIndex(
		static_cast<int>(switcher->autoStartEvent));

	// Set up status control
	auto statusControl = new StatusControl(this, true);
	ui->statusLayout->addWidget(statusControl->StatusPrefixLabel(), 1, 0);
	auto tmp = new QHBoxLayout;
	tmp->addWidget(statusControl->StatusLabel());
	tmp->addStretch();
	ui->statusLayout->addLayout(tmp, 1, 1);
	ui->statusLayout->addWidget(statusControl->Button(), 2, 1);
	// Hide the now empty invisible shell of the statusControl widget
	// as otherwise it could block user input
	statusControl->hide();
	setupGeneralTabInactiveWarning(ui->tabWidget);

	// Adjust tab order
	setTabOrder(ui->checkInterval, statusControl->Button());
	setTabOrder(statusControl->Button(), ui->startupBehavior);
	setTabOrder(ui->importSettings, ui->cooldownTime);
	setTabOrder(ui->cooldownTime, ui->noMatchDontSwitch);

	MinimizeSizeOfColumn(ui->statusLayout, 0);
	setWindowTitle(windowTitle() + " - " + g_GIT_TAG);
}

} // namespace advss
