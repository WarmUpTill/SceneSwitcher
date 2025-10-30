#include "advanced-scene-switcher.hpp"
#include "file-selection.hpp"
#include "filter-combo-box.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "macro-settings.hpp"
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

void AdvSceneSwitcher::on_logLevel_currentIndexChanged(int idx)
{
	if (loading) {
		return;
	}
	SetLogLevel(static_cast<LogLevel>(ui->logLevel->itemData(idx).toInt()));
}

void AdvSceneSwitcher::on_autoStartEvent_currentIndexChanged(int index)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->autoStartEvent = static_cast<SwitcherData::AutoStart>(index);
}

void AdvSceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->interval = value;

	SetCheckIntervalTooLowVisibility();
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
	ui->macroEdit->SetMacro(nullptr); // Trigger saving of splitter states

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

void AdvSceneSwitcher::on_disableMacroWidgetCache_stateChanged(int state)
{
	if (loading) {
		return;
	}

	switcher->disableMacroWidgetCache = state;
	MacroSegmentList::SetCachingEnabled(!state);
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
	       name == obs_module_text("AdvSceneSwitcher.pauseTab.title");
}

void AdvSceneSwitcher::on_hideLegacyTabs_stateChanged(int state)
{
	switcher->hideLegacyTabs = state;

	for (int idx = 0; idx < ui->tabWidget->count(); idx++) {
		if (isLegacyTab(ui->tabWidget->tabText(idx))) {
			ui->tabWidget->setTabVisible(idx, !state);
		}
	}

	// Changing priority of legacy tabs will very likely not be necessary if
	// the legacy tabs are hidden
	ui->priorityBox->setVisible(!switcher->hideLegacyTabs);
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
	OBSDataArrayAutoRelease mqttConnections =
		obs_data_get_array(data, "mqttConnections");

	auto isNotEmpty = [](obs_data_array *array) {
		return obs_data_array_count(array) > 0;
	};

	return isNotEmpty(twitchTokens) || isNotEmpty(websocketConnections) ||
	       isNotEmpty(mqttConnections);
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

	// We have to make sure to that no macro is currently being edited while
	// the new settings are loaded
	ui->macros->clearSelection();

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
		switcher->Start();
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
	ClearPostLoadSteps();

	// Needs to be loaded before any entries which might rely on scene group
	// selections to be available.
	loadSceneGroups(obj);
	LoadVariables(obj);

	RunLoadSteps(obj);

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
	LoadGeneralSettings(obj);
	LoadHotkeys(obj);
	LoadUISettings(obj);

	RunAndClearPostLoadSteps();

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
	SaveGeneralSettings(obj);
	SaveHotkeys(obj);
	SaveUISettings(obj);
	SaveVersion(obj, g_GIT_SHA1);

	RunSaveSteps(obj);
}

void SwitcherData::SaveGeneralSettings(obs_data_t *obj)
{
	obs_data_set_int(obj, "interval", interval);

	OBSDataAutoRelease noMatchScene = obs_data_create();
	nonMatchingScene.Save(noMatchScene);
	obs_data_set_obj(obj, "noMatchScene", noMatchScene);
	obs_data_set_int(obj, "switch_if_not_matching",
			 static_cast<int>(switchIfNotMatching));
	noMatchDelay.Save(obj, "noMatchDelay");

	cooldown.Save(obj, "cooldown");
	obs_data_set_bool(obj, "enableCooldown", enableCooldown);

	obs_data_set_bool(obj, "active", sceneCollectionStop ? true : !stop);
	sceneCollectionStop = false;
	obs_data_set_int(obj, "startup_behavior",
			 static_cast<int>(startupBehavior));

	OBSDataAutoRelease autoStart = obs_data_create();
	obs_data_set_int(autoStart, "event", static_cast<int>(autoStartEvent));
	obs_data_set_bool(autoStart, "useAutoStartScene", useAutoStartScene);
	autoStartScene.Save(autoStart);
	autoStartSceneName.Save(autoStart, "name");
	autoStartSceneRegex.Save(autoStart);
	obs_data_set_obj(obj, "autoStart", autoStart);

	SaveLogLevel(obj);

	obs_data_set_bool(obj, "showSystemTrayNotifications",
			  showSystemTrayNotifications);
	obs_data_set_bool(obj, "disableHints", disableHints);
	obs_data_set_bool(obj, "disableFilterComboboxFilter",
			  disableFilterComboboxFilter);
	obs_data_set_bool(obj, "disableMacroWidgetCache",
			  disableMacroWidgetCache);
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

	if (obs_data_has_user_value(obj, "noMatchScene")) {
		OBSDataAutoRelease noMatchScene =
			obs_data_get_obj(obj, "noMatchScene");
		nonMatchingScene.Load(noMatchScene);
	} else {
		nonMatchingScene.Load(obj, "non_matching_scene");
	}
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

	OBSDataAutoRelease autoStart = obs_data_get_obj(obj, "autoStart");
	autoStartEvent = static_cast<AutoStart>(
		obs_data_has_user_value(obj, "autoStart")
			? obs_data_get_int(autoStart, "event")
			: obs_data_get_int(obj, "autoStartEvent"));
	useAutoStartScene = obs_data_get_bool(autoStart, "useAutoStartScene");
	autoStartScene.Load(autoStart);
	autoStartSceneName.Load(autoStart, "name");
	autoStartSceneRegex.Load(autoStart);

	LoadLogLevel(obj);

	showSystemTrayNotifications =
		obs_data_get_bool(obj, "showSystemTrayNotifications");
	disableHints = obs_data_get_bool(obj, "disableHints");
	disableFilterComboboxFilter =
		obs_data_get_bool(obj, "disableFilterComboboxFilter");
	disableMacroWidgetCache =
		obs_data_get_bool(obj, "disableMacroWidgetCache");
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

	auto noMatchScene = nonMatchingScene.GetScene(false);
	if (switchIfNotMatching == NoMatchBehavior::SWITCH && noMatchScene) {
		match = true;
		scene = noMatchScene;
		transition = nullptr;
	}
	if (switchIfNotMatching == NoMatchBehavior::RANDOM_SWITCH) {
		match = checkRandom(scene, transition, sleep);
	}
}

void SwitcherData::CheckAutoStart()
{
	if (!useAutoStartScene) {
		return;
	}

	bool shouldStartPlugin = false;
	if (autoStartSceneRegex.Enabled()) {
		const auto currentSceneName = GetWeakSourceName(currentScene);
		shouldStartPlugin = autoStartSceneRegex.Matches(
			currentSceneName, autoStartSceneName);
	} else {
		shouldStartPlugin = autoStartScene.GetScene(false) ==
				    currentScene;
	}

	if (shouldStartPlugin) {
		Start();
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

void AdvSceneSwitcher::SetCheckIntervalTooLowVisibility() const
{
	auto macro = GetMacroWithInvalidConditionInterval();
	if (!macro) {
		ui->checkIntervalTooLowWarning->hide();
		return;
	}

	const QString labelTextFormat(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.checkIntervalTooLow"));
	const QString labelTooltipFormat(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.checkIntervalTooLow.tooltip"));
	const QString name = QString::fromStdString(macro->Name());
	const QString duration = QString::fromStdString(
		macro->GetCustomConditionCheckInterval().ToString());

	ui->checkIntervalTooLowWarning->setText(labelTextFormat.arg(name));
	ui->checkIntervalTooLowWarning->setToolTip(
		labelTooltipFormat.arg(name).arg(duration).arg(name));
	ui->checkIntervalTooLowWarning->show();
}

void AdvSceneSwitcher::SetupGeneralTab()
{
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
	ui->noMatchSwitchScene->SetScene(switcher->nonMatchingScene);
	ui->noMatchSwitchScene->LockToMainCanvas();

	connect(ui->noMatchSwitchScene, &SceneSelectionWidget::SceneChanged,
		this, [this](const SceneSelection &scene) {
			if (loading) {
				return;
			}
			std::lock_guard<std::mutex> lock(switcher->m);
			switcher->nonMatchingScene = scene;
		});

	DurationSelection *noMatchDelay = new DurationSelection();
	noMatchDelay->SetDuration(switcher->noMatchDelay);
	noMatchDelay->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.onNoMatchDelay.tooltip"));
	ui->noMatchLayout->addWidget(noMatchDelay);
	QWidget::connect(noMatchDelay,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(NoMatchDelayDurationChanged(const Duration &)));

	ui->checkInterval->setValue(switcher->interval);
	SetCheckIntervalTooLowVisibility();

	ui->enableCooldown->setChecked(switcher->enableCooldown);
	ui->cooldownTime->setEnabled(switcher->enableCooldown);
	ui->cooldownTime->SetDuration(switcher->cooldown);
	ui->cooldownTime->setToolTip(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.cooldownHint"));
	QWidget::connect(ui->cooldownTime,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(CooldownDurationChanged(const Duration &)));

	PopulateLogLevelSelection(ui->logLevel);
	ui->logLevel->setCurrentIndex(
		ui->logLevel->findData(static_cast<int>(GetLogLevel())));

	ui->saveWindowGeo->setChecked(switcher->saveWindowGeo);
	ui->showTrayNotifications->setChecked(
		switcher->showSystemTrayNotifications);
	ui->uiHintsDisable->setChecked(switcher->disableHints);
	ui->disableComboBoxFilter->setChecked(
		switcher->disableFilterComboboxFilter);
	FilterComboBox::SetFilterBehaviourEnabled(
		!switcher->disableFilterComboboxFilter);
	ui->disableMacroWidgetCache->setChecked(
		switcher->disableMacroWidgetCache);
	MacroSegmentList::SetCachingEnabled(!switcher->disableMacroWidgetCache);
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
	ui->autoStartSceneEnable->setChecked(switcher->useAutoStartScene);
	ui->autoStartScene->SetScene(switcher->autoStartScene);
	ui->autoStartScene->LockToMainCanvas();
	ui->autoStartSceneName->setText(switcher->autoStartSceneName);
	ui->autoStartSceneNameRegex->SetRegexConfig(
		switcher->autoStartSceneRegex);

	const auto setupAutoStartSceneLayoutVisibility = [this](bool useRegex) {
		ui->autoStartSceneName->setVisible(useRegex);
		ui->autoStartScene->setVisible(!useRegex);
		if (useRegex) {
			RemoveStretchIfPresent(ui->autoStartSceneLayout);
		} else {
			AddStretchIfNecessary(ui->autoStartSceneLayout);
		}
	};
	setupAutoStartSceneLayoutVisibility(
		switcher->autoStartSceneRegex.Enabled());

	const auto setupAutoStartSceneWidgetState =
		[this](bool useAutoStartScene) {
			ui->autoStartScene->setEnabled(useAutoStartScene);
			ui->autoStartSceneName->setEnabled(useAutoStartScene);
			ui->autoStartSceneNameRegex->setEnabled(
				useAutoStartScene);
		};
	setupAutoStartSceneWidgetState(switcher->useAutoStartScene);

	connect(ui->autoStartSceneEnable, &QCheckBox::stateChanged, this,
		[this, setupAutoStartSceneWidgetState](int enabled) {
			if (loading) {
				return;
			}
			std::lock_guard<std::mutex> lock(switcher->m);
			switcher->useAutoStartScene = enabled;
			setupAutoStartSceneWidgetState(enabled);
		});

	connect(ui->autoStartScene, &SceneSelectionWidget::SceneChanged, this,
		[this](const SceneSelection &scene) {
			if (loading) {
				return;
			}
			std::lock_guard<std::mutex> lock(switcher->m);
			switcher->autoStartScene = scene;
			switcher->CheckAutoStart();
		});
	connect(ui->autoStartSceneName, &VariableLineEdit::editingFinished,
		this, [this]() {
			if (loading) {
				return;
			}
			std::lock_guard<std::mutex> lock(switcher->m);
			switcher->autoStartSceneName =
				ui->autoStartSceneName->text().toStdString();
			switcher->CheckAutoStart();
		});
	connect(ui->autoStartSceneNameRegex,
		&RegexConfigWidget::RegexConfigChanged, this,
		[this, setupAutoStartSceneLayoutVisibility](
			const RegexConfig &regex) {
			if (loading) {
				return;
			}
			std::lock_guard<std::mutex> lock(switcher->m);
			switcher->autoStartSceneRegex = regex;
			setupAutoStartSceneLayoutVisibility(regex.Enabled());
			switcher->CheckAutoStart();
		});

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
