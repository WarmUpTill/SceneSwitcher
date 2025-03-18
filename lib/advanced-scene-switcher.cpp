#include "advanced-scene-switcher.hpp"
#include "backup.hpp"
#include "curl-helper.hpp"
#include "log-helper.hpp"
#include "macro-helpers.hpp"
#include "obs-module-helper.hpp"
#include "path-helpers.hpp"
#include "platform-funcs.hpp"
#include "scene-switch-helpers.hpp"
#include "source-helpers.hpp"
#include "status-control.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "tab-helpers.hpp"
#include "utility.hpp"
#include "version.h"
#include "websocket-api.hpp"

#include <obs-frontend-api.h>
#include <QAction>
#include <QDirIterator>
#include <QMainWindow>
#include <QTextStream>
#include <regex>

namespace advss {

AdvSceneSwitcher *AdvSceneSwitcher::window = nullptr;

/******************************************************************************
 * Create the Advanced Scene Switcher settings window
 ******************************************************************************/
AdvSceneSwitcher::AdvSceneSwitcher(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui_AdvSceneSwitcher)
{
	switcher->settingsWindowOpened = true;
	ui->setupUi(this);
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->Prune();
	LoadUI();
}

AdvSceneSwitcher::~AdvSceneSwitcher()
{
	if (switcher) {
		switcher->settingsWindowOpened = false;
		SaveLastOpenedTab(ui->tabWidget);
	}
}

static bool translationAvailable()
{
	return !!strcmp(obs_module_text("AdvSceneSwitcher.pluginName"),
			"AdvSceneSwitcher.pluginName");
}

static void DisplayMissingDependencyWarning()
{
	if (!switcher->warnPluginLoadFailure ||
	    switcher->loadFailureLibs.isEmpty()) {
		return;
	}

	QString failedLibsString = "<ul>";
	for (const auto &lib : switcher->loadFailureLibs) {
		failedLibsString += "<li>" + lib + "</li>";
	}
	failedLibsString += "</ul>";
	QString warning(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.warnPluginLoadFailureMessage"));
	DisplayMessage(warning.arg(failedLibsString));

	// Only display the warning once per plugin load
	switcher->loadFailureLibs.clear();
}

static void DisplayMissingDataDirWarning()
{
	if (translationAvailable()) {
		return;
	}

	QString msg = "Failed to find plug-in's 'data' directory.\n"
		      "Please check installation instructions!\n\n"
		      "Data most likely expected at:\n\n";
#ifdef _WIN32

	msg += QDir::currentPath();
	msg += "/";
#endif
	msg += obs_get_module_data_path(obs_current_module());
	DisplayMessage(msg);
}

bool CanCreateDefaultAction();
bool CanCreateDefaultCondition();

static void DisplayCorruptedInstallWarning()
{
	if (CanCreateDefaultAction() && CanCreateDefaultCondition()) {
		return;
	}

	DisplayMessage(obs_module_text(
		"AdvSceneSwitcher.generalTab.generalBehavior.warnCorruptedInstallMessage"));
}

void AdvSceneSwitcher::LoadUI()
{
	DisplayMissingDataDirWarning();
	DisplayMissingDependencyWarning();
	DisplayCorruptedInstallWarning();

	SetupGeneralTab();
	SetupTitleTab();
	SetupExecutableTab();
	SetupRegionTab();
	SetupPauseTab();
	SetupSequenceTab();
	SetupTransitionsTab();
	SetupIdleTab();
	SetupRandomTab();
	SetupMediaTab();
	SetupFileTab();
	SetupTimeTab();
	SetupAudioTab();
	SetupVideoTab();
	SetupSceneGroupTab();
	SetupMacroTab();
	SetupOtherTabs(ui->tabWidget);

	SetDeprecationWarnings();
	SetTabOrder(ui->tabWidget);
	SetCurrentTab(ui->tabWidget);
	RestoreWindowGeo();
	CheckFirstTimeSetup();

	loading = false;
}

bool AdvSceneSwitcher::eventFilter(QObject *obj, QEvent *event)
{
	auto eventType = event->type();
	if (obj == ui->macroElseActions && eventType == QEvent::Resize) {
		QResizeEvent *resizeEvent = static_cast<QResizeEvent *>(event);

		if (resizeEvent->size().height() == 0) {
			SetElseActionsStateToHidden();
			return QDialog::eventFilter(obj, event);
		}

		SetElseActionsStateToVisible();
	} else if (eventType == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
		auto pressedKey = keyEvent->key();

		if (obj == ui->macros && ui->macros->isVisible()) {
			if (pressedKey == Qt::Key_F2) {
				RenameSelectedMacro();
			} else if (pressedKey == Qt::Key_Delete) {
				RemoveSelectedMacros();
			}
		}
	}

	return QDialog::eventFilter(obj, event);
}

/******************************************************************************
 * Saving and loading
 ******************************************************************************/

static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (!switcher) {
		return;
	}

	if (saving) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->Prune();
		OBSDataAutoRelease data = obs_data_create();
		switcher->SaveSettings(data);
		obs_data_set_obj(save_data, "advanced-scene-switcher", data);
	} else {
		// Stop the scene switcher at least once to
		// avoid scene duplication issues with scene collection changes
		switcher->Stop();

		OBSDataAutoRelease data =
			obs_data_get_obj(save_data, "advanced-scene-switcher");
		if (!data) {
			data = obs_data_create();
		}

		switcher->m.lock();
		if (switcher->VersionChanged(data, g_GIT_SHA1)) {
			auto json = obs_data_get_json(data);
			static QString jsonQString = json ? json : "";
			std::thread t([]() {
				obs_queue_task(
					OBS_TASK_UI,
					[](void *) {
						AskForBackup(jsonQString);
					},
					nullptr, false);
			});
			t.detach();
		}

		switcher->LoadSettings(data);
		switcher->m.unlock();

		if (!switcher->stop) {
			switcher->Start();
		}
	}
}

/******************************************************************************
 * Main switcher thread
 ******************************************************************************/
void SwitcherData::Thread()
{
	blog(LOG_INFO, "started");
	int sleep = 0;
	int linger = 0;
	std::chrono::milliseconds duration;
	auto startTime = std::chrono::high_resolution_clock::now();
	auto endTime = std::chrono::high_resolution_clock::now();
	switcher->firstIntervalAfterStop = true;

	while (true) {
		std::unique_lock<std::mutex> lock(m);
		mainLoopLock = &lock;

		bool match = false;
		OBSWeakSource scene;
		OBSWeakSource transition;
		// The previous scene might have changed during the linger duration,
		// if a longer transition is used than the configured check interval
		bool setPrevSceneAfterLinger = false;
		bool macroMatch = false;
		endTime = std::chrono::high_resolution_clock::now();
		auto runTime =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				endTime - startTime);

		if (sleep) {
			duration = std::chrono::milliseconds(sleep);
		} else {
			duration = std::chrono::milliseconds(interval) +
				   std::chrono::milliseconds(linger) - runTime;
			if (duration.count() < 1) {
				vblog(LOG_INFO,
				      "detected busy loop - refusing to sleep less than 1ms");
				duration = std::chrono::milliseconds(10);
			}
		}

		vblog(LOG_INFO, "try to sleep for %ld",
		      (long int)duration.count());
		SetWaitScene();
		cv.wait_for(lock, duration);

		startTime = std::chrono::high_resolution_clock::now();
		sleep = 0;
		linger = 0;

		Prune();
		if (stop) {
			break;
		}
		if (checkPause()) {
			continue;
		}
		SetPreconditions();
		match = CheckForMatch(scene, transition, linger,
				      setPrevSceneAfterLinger, macroMatch);
		if (stop) {
			break;
		}
		CheckNoMatchSwitch(match, scene, transition, sleep);
		checkSwitchCooldown(match);

		if (linger) {
			duration = std::chrono::milliseconds(linger);
			vblog(LOG_INFO, "sleep for %ld before switching scene",
			      (long int)duration.count());

			SetWaitScene();
			cv.wait_for(lock, duration);

			if (stop) {
				break;
			}

			if (SceneChangedDuringWait()) {
				vblog(LOG_INFO,
				      "scene was changed manually - ignoring match");

				match = false;
				linger = 0;
			} else if (setPrevSceneAfterLinger) {
				scene = previousScene;
			}
		}

		RunIntervalResetSteps();

		if (match) {
			if (macroMatch) {
				RunMacros();
			} else {
				SwitchScene({scene, transition, 0});
			}
		}

		writeSceneInfoToFile();
		switcher->firstInterval = false;
		switcher->firstIntervalAfterStop = false;
	}

	mainLoopLock = nullptr;
	blog(LOG_INFO, "stopped");
}

void SwitcherData::SetPreconditions()
{
	// Window title
	lastTitle = currentTitle;
	std::string title;
	GetCurrentWindowTitle(title);
	for (auto &window : ignoreWindowsSwitches) {
		bool equals = (title == window);
		bool matches = false;
		if (!equals) {
			try {
				std::regex expr(window);
				matches = std::regex_match(title, expr);

			} catch (const std::regex_error &) {
			}
		}
		if (equals || matches) {
			title = lastTitle;
			break;
		}
	}
	currentTitle = title;

	// Process name
	GetForegroundProcessName(currentForegroundProcess);

	// Macro
	InvalidateMacroTempVarValues();
}

bool SwitcherData::CheckForMatch(OBSWeakSource &scene,
				 OBSWeakSource &transition, int &linger,
				 bool &setPrevSceneAfterLinger,
				 bool &macroMatch)
{
	bool match = false;

	if (uninterruptibleSceneSequenceActive) {
		match = checkSceneSequence(scene, transition, linger,
					   setPrevSceneAfterLinger);
		if (match) {
			return match;
		}
	}

	for (int switchFuncName : functionNamesByPriority) {
		switch (switchFuncName) {
		case read_file_func:
			match = checkSwitchInfoFromFile(scene, transition) ||
				checkFileContent(scene, transition);
			break;
		case idle_func:
			match = checkIdleSwitch(scene, transition);
			break;
		case exe_func:
			match = checkExeSwitch(scene, transition);
			break;
		case screen_region_func:
			match = checkScreenRegionSwitch(scene, transition);
			break;
		case window_title_func:
			match = checkWindowTitleSwitch(scene, transition);
			break;
		case round_trip_func:
			match = checkSceneSequence(scene, transition, linger,
						   setPrevSceneAfterLinger);
			break;
		case media_func:
			match = checkMediaSwitch(scene, transition);
			break;
		case time_func:
			match = checkTimeSwitch(scene, transition);
			break;
		case audio_func:
			match = checkAudioSwitch(scene, transition);
			break;
		case video_func:
			match = checkVideoSwitch(scene, transition);
			break;
		case macro_func:
			if (CheckMacros()) {
				match = true;
				macroMatch = true;
			}
			break;
		}

		if (stop) {
			return false;
		}
		if (match) {
			break;
		}
	}
	return match;
}

static void ResetMacros()
{
	for (auto &m : GetMacros()) {
		ResetMacroRunCount(m.get());
		ResetMacroConditionTimers(m.get());
	}
}

void AutoStartActionQueues();

void SwitcherData::Start()
{
	if (!(th && th->isRunning())) {
		RunIntervalResetSteps();
		ResetMacros();
		AutoStartActionQueues();

		stop = false;
		th = new SwitcherThread();
		th->start((QThread::Priority)threadPriority);

		// Will be overwritten quickly but might be useful
		writeToStatusFile("Advanced Scene Switcher running");
		SendWebsocketVendorEvent("AdvancedSceneSwitcherStarted",
					 nullptr);
	}

	if (showSystemTrayNotifications) {
		DisplayTrayMessage(
			obs_module_text("AdvSceneSwitcher.pluginName"),
			obs_module_text("AdvSceneSwitcher.running"));
	}
}

void CloseAllInputDialogs();
void StopAndClearAllActionQueues();

void SwitcherData::Stop()
{
	if (th && th->isRunning()) {
		stop = true;
		cv.notify_all();
		SetMacroAbortWait(true);
		GetMacroWaitCV().notify_all();
		GetMacroTransitionCV().notify_all();
		StopAllMacros();
		StopAndClearAllActionQueues();
		CloseAllInputDialogs();
		th->wait();
		delete th;
		th = nullptr;
		writeToStatusFile("Advanced Scene Switcher stopped");
		if (!obsIsShuttingDown) {
			SendWebsocketVendorEvent("AdvancedSceneSwitcherStopped",
						 nullptr);
		}
	}

	if (showSystemTrayNotifications) {
		DisplayTrayMessage(
			obs_module_text("AdvSceneSwitcher.pluginName"),
			obs_module_text("AdvSceneSwitcher.stopped"));
	}
}

void SwitcherData::SetWaitScene()
{
	waitScene = obs_frontend_get_current_scene();
	obs_source_release(waitScene);
}

bool SwitcherData::SceneChangedDuringWait()
{
	OBSSourceAutoRelease currentSource = obs_frontend_get_current_scene();
	if (!currentSource) {
		return true;
	}
	return (waitScene && currentSource != waitScene);
}

// Relies on the fact that switcher->currentScene will only be updated on event
// OBS_FRONTEND_EVENT_SCENE_CHANGED but obs_frontend_get_current_scene() will
// already return the scene to be transitioned to.
bool SwitcherData::AnySceneTransitionStarted()
{
	OBSSourceAutoRelease currentSceneSrouce =
		obs_frontend_get_current_scene();
	OBSWeakSourceAutoRelease currentScene =
		obs_source_get_weak_source(currentSceneSrouce);
	bool ret = switcher->currentScene != currentScene;
	return ret;
}

/******************************************************************************
 * OBS module setup
 ******************************************************************************/
extern "C" EXPORT void FreeSceneSwitcher()
{
	PlatformCleanup();
	RunPluginCleanupSteps();

	delete switcher;
	switcher = nullptr;
}

static void handleSceneChange()
{
	// Stop waiting if scene was changed
	if (switcher->SceneChangedDuringWait()) {
		switcher->cv.notify_one();
	}

	// Set current and previous scene
	OBSSourceAutoRelease source = obs_frontend_get_current_scene();
	OBSWeakSourceAutoRelease ws = obs_source_get_weak_source(source);

	if (ws && ws != switcher->currentScene) {
		switcher->previousScene = switcher->currentScene;
		switcher->currentScene = ws;
		vblog(LOG_INFO, "current scene:  %s",
		      GetWeakSourceName(switcher->currentScene).c_str());
		vblog(LOG_INFO, "previous scene: %s",
		      GetWeakSourceName(switcher->previousScene).c_str());
	}

	switcher->checkDefaultSceneTransitions();
}

static void setLiveTime()
{
	switcher->liveTime = QDateTime::currentDateTime();
}

static void resetLiveTime()
{
	switcher->liveTime = QDateTime();
}

static void checkAutoStartRecording()
{
	if (switcher->autoStartEvent == SwitcherData::AutoStart::RECORDING ||
	    switcher->autoStartEvent ==
		    SwitcherData::AutoStart::RECORINDG_OR_STREAMING)
		switcher->Start();
}

static void checkAutoStartStreaming()
{
	if (switcher->autoStartEvent == SwitcherData::AutoStart::STREAMING ||
	    switcher->autoStartEvent ==
		    SwitcherData::AutoStart::RECORINDG_OR_STREAMING)
		switcher->Start();
}

static void handleTransitionEnd()
{
	GetMacroTransitionCV().notify_all();
}

static void handleShutdown()
{
	if (!switcher) {
		return;
	}
	switcher->obsIsShuttingDown = true;
	if (ShutdownCheckIsNecessary()) {
		switcher->Stop();
		CheckMacros();
		RunMacros();

		// Unfortunately this will not work as OBS will now allow saving
		// the scene collection data at this point, So any OBS specific
		// changes done during shutdown will be lost
		//
		// TODO: Look for a way to possibly resolve this
		obs_frontend_save();
	}

	BackupSettingsOfCurrentVersion();
}

static void handleSceneCollectionChanging()
{
	if (switcher->settingsWindowOpened) {
		AdvSceneSwitcher::window->close();
	}
	if (!switcher->stop) {
		switcher->sceneColletionStop = true;
		switcher->Stop();
	}
}

static void handleSceneCollectionCleanup()
{
	// Reset the plugin to default settings to avoid settings being carried
	// over to newly created scene collections.

	// Skip this step for the initial OBS startup.
	// OBSBasic::LoadData() will call ClearSceneData() which will trigger
	// this function to be called.
	// This would result in the plugin being started with default settings
	// before the "real" settings are loaded.
	static bool isOBSStartup = true;
	if (isOBSStartup) {
		isOBSStartup = false;
		return;
	}

	SaveSceneSwitcher(nullptr, false, nullptr);
}

// Note to future self:
// be careful using switcher->m here as there is potential for deadlocks when using
// frontend functions such as obs_frontend_set_current_scene()
static void OBSEvent(enum obs_frontend_event event, void *switcher)
{
	if (!switcher) {
		return;
	}

	switch (event) {
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
		// Note: We are intentionally not listening for
		// OBS_FRONTEND_EVENT_EXIT here as at that point all scene
		// collection data will already have been cleared and thus all
		// macros will have already been cleared
		handleShutdown();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		handleSceneChange();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		setLiveTime();
		checkAutoStartRecording();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		setLiveTime();
		checkAutoStartStreaming();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		resetLiveTime();
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		handleTransitionEnd();
		break;
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(27, 2, 0)
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
		handleSceneCollectionChanging();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		handleSceneCollectionCleanup();
		break;
#endif
	default:
		break;
	}
}

static void LoadPlugins()
{
	QFileInfo libPath(
		QString(obs_get_module_binary_path(obs_current_module())));
	QString pluginDir(libPath.absolutePath() + "/" ADVSS_PLUGIN_FOLDER);
#ifdef _WIN32
	QString libPattern = "*.dll";
	SetDllDirectory(pluginDir.toStdWString().c_str());
#else
	QString libPattern = "*.so";
#endif
	QDirIterator it(pluginDir, QStringList() << libPattern, QDir::Files);
	while (it.hasNext()) {
		auto file = it.next();
		blog(LOG_INFO, "attempting to load \"%s\"",
		     file.toStdString().c_str());
		auto lib = new QLibrary(file, nullptr);
		if (lib->load()) {
			blog(LOG_INFO, "successfully loaded \"%s\"",
			     file.toStdString().c_str());
		} else {
			blog(LOG_WARNING, "failed to load \"%s\": %s",
			     file.toStdString().c_str(),
			     lib->errorString().toStdString().c_str());
			switcher->loadFailureLibs << file;
		}
	}
}

void OpenSettingsWindow()
{
	if (switcher->settingsWindowOpened) {
		AdvSceneSwitcher::window->show();
		AdvSceneSwitcher::window->raise();
		AdvSceneSwitcher::window->activateWindow();
	} else {
		AdvSceneSwitcher::window =
			new AdvSceneSwitcher(static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));
		AdvSceneSwitcher::window->setAttribute(Qt::WA_DeleteOnClose);
		AdvSceneSwitcher::window->show();
	}
}

void AdvSceneSwitcher::HighlightMacroSettingsButton(bool enable)
{
	static QObject *highlight = nullptr;
	if ((highlight && enable) || (!highlight && !enable)) {
		return;
	}

	if (highlight && !enable) {
		highlight->deleteLater();
		highlight = nullptr;
		return;
	}

	if (!HighlightUIElementsEnabled()) {
		return;
	}

	highlight = HighlightWidget(ui->macroSettings, Qt::green);
}

void HighlightMacroSettingsButton(bool enable)
{
	auto window = GetSettingsWindow();
	if (!window) {
		return;
	}
	static_cast<AdvSceneSwitcher *>(window)->HighlightMacroSettingsButton(
		enable);
}

void SetupActionQueues();

extern "C" EXPORT void InitSceneSwitcher(obs_module_t *module,
					 translateFunc translate)
{
	blog(LOG_INFO, "version: %s", g_GIT_TAG);
	blog(LOG_INFO, "version: %s", g_GIT_SHA1);

	switcher = new SwitcherData(module, translate);

	PlatformInit();
	LoadPlugins();
	SetupDock();
	SetupActionQueues();

	RunPluginInitSteps();

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, switcher);

	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("AdvSceneSwitcher.pluginName"));
	action->connect(action, &QAction::triggered, OpenSettingsWindow);
}

} // namespace advss
