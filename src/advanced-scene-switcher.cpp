#include <QMainWindow>
#include <QAction>
#include <QFileDialog>
#include <regex>
#include <filesystem>

#include <obs-module.h>
#include <obs-frontend-api.h>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/curl-helper.hpp"
#include "headers/utility.hpp"
#include "headers/version.h"

SwitcherData *switcher = nullptr;

/******************************************************************************
 * Create the Advanced Scene Switcher settings window
 ******************************************************************************/
AdvSceneSwitcher::AdvSceneSwitcher(QWidget *parent)
	: QDialog(parent), ui(new Ui_AdvSceneSwitcher)
{
	switcher->settingsWindowOpened = true;
	ui->setupUi(this);

	std::lock_guard<std::mutex> lock(switcher->m);

	switcher->Prune();
	loadUI();
}

AdvSceneSwitcher::~AdvSceneSwitcher()
{
	if (switcher) {
		switcher->settingsWindowOpened = false;
	}
}

bool translationAvailable()
{
	return !!strcmp(obs_module_text("AdvSceneSwitcher.pluginName"),
			"AdvSceneSwitcher.pluginName");
}

void AdvSceneSwitcher::loadUI()
{
	if (!translationAvailable()) {
		QString msg = "Failed to find plug-in's 'data' directory.\n"
			      "Please check installation instructions!\n\n"
			      "Data most likely expected at:\n\n";
#ifdef _WIN32
		msg += QString::fromStdString(
			(std::filesystem::current_path().string()));
		msg += "/";
#endif
		msg += obs_get_module_data_path(obs_current_module());
		(void)DisplayMessage(msg);
	}

#if __APPLE__
	setMinimumHeight(700);
#endif
	setupGeneralTab();
	setupTitleTab();
	setupExecutableTab();
	setupRegionTab();
	setupPauseTab();
	setupSequenceTab();
	setupTransitionsTab();
	setupIdleTab();
	setupRandomTab();
	setupMediaTab();
	setupFileTab();
	setupTimeTab();
	setupAudioTab();
	setupVideoTab();
	setupNetworkTab();
	setupSceneGroupTab();
	setupTriggerTab();
	setupMacroTab();

	setTabOrder();
	restoreWindowGeo();

	loading = false;
}

/******************************************************************************
 * Saving and loading
 ******************************************************************************/
static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->Prune();
		obs_data_t *obj = obs_data_create();
		switcher->saveSettings(obj);
		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);
		obs_data_release(obj);
	} else {
		// Stop the scene switcher at least once to
		// avoid scene duplication issues with scene collection changes
		bool start = !switcher->stop;
		switcher->Stop();

		switcher->m.lock();
		obs_data_t *obj =
			obs_data_get_obj(save_data, "advanced-scene-switcher");
		if (!obj) {
			obj = obs_data_create();
		}
		if (switcher->versionChanged(obj, g_GIT_SHA1)) {
			AskForBackup(obj);
		}
		switcher->loadSettings(obj);
		obs_data_release(obj);
		switcher->m.unlock();

		if (start) {
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

	while (true) {
		std::unique_lock<std::mutex> lock(m);

		bool match = false;
		OBSWeakSource scene;
		OBSWeakSource transition;
		// The previous scene might have changed during the linger duration,
		// if a longer transition is used than the configured check interval
		bool setPrevSceneAfterLinger = false;
		bool macroMatch = false;
		macroSceneSwitched = false;
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
				blog(LOG_INFO,
				     "detected busy loop - refusing to sleep less than 1ms");
				duration = std::chrono::milliseconds(50);
			}
		}

		vblog(LOG_INFO, "try to sleep for %ld", duration.count());
		setWaitScene();
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
		setPreconditions();
		match = checkForMatch(scene, transition, linger,
				      setPrevSceneAfterLinger, macroMatch);
		if (stop) {
			break;
		}
		checkNoMatchSwitch(match, scene, transition, sleep);
		checkSwitchCooldown(match);

		if (linger) {
			duration = std::chrono::milliseconds(linger);
			vblog(LOG_INFO, "sleep for %ld before switching scene",
			      duration.count());

			setWaitScene();
			cv.wait_for(lock, duration);

			if (stop) {
				break;
			}

			if (sceneChangedDuringWait()) {
				vblog(LOG_INFO,
				      "scene was changed manually - ignoring match");

				match = false;
				linger = 0;
			} else if (setPrevSceneAfterLinger) {
				scene = previousScene;
			}
		}

		// After this point we will call frontend functions like
		// obs_frontend_set_current_scene() and
		// obs_frontend_set_current_transition()
		//
		// During this time SaveSceneSwitcher() could be called
		// leading to a deadlock with the frontend function being stuck
		// in QMetaObject::invokeMethod() holding the mutex and
		// OBS being stuck in SaveSceneSwitcher().
		//
		// So we have to unlock() risking race conditions as these are
		// less frequent than the above described deadlock
		lock.unlock();

		if (match) {
			if (macroMatch) {
				runMacros();
			} else {
				switchScene({scene, transition, 0});
			}
		}

		writeSceneInfoToFile();
	}

	blog(LOG_INFO, "stopped");
}

void SwitcherData::setPreconditions()
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

	// Cursor
	std::pair<int, int> cursorPos = getCursorPos();
	cursorPosChanged = cursorPos.first != switcher->lastCursorPos.first ||
			   cursorPos.second != switcher->lastCursorPos.second;
	lastCursorPos = getCursorPos();
}

bool SwitcherData::checkForMatch(OBSWeakSource &scene,
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
			if (checkMacros()) {
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

void switchScene(const sceneSwitchInfo &sceneSwitch)
{
	if (!sceneSwitch.scene && switcher->verbose) {
		blog(LOG_INFO, "nothing to switch to");
		return;
	}

	obs_source_t *source = obs_weak_source_get_source(sceneSwitch.scene);
	obs_source_t *currentSource = obs_frontend_get_current_scene();

	if (source && source != currentSource) {
		transitionData currentTransitionData;
		setNextTransition(sceneSwitch, currentSource,
				  currentTransitionData);
		obs_frontend_set_current_scene(source);
		if (switcher->transitionOverrideOverride) {
			restoreTransitionOverride(source,
						  currentTransitionData);
		}

		if (switcher->verbose) {
			blog(LOG_INFO, "switched scene");
		}

		if (switcher->networkConfig.ShouldSendSceneChange()) {
			switcher->server.sendMessage(sceneSwitch);
		}
	}
	obs_source_release(currentSource);
	obs_source_release(source);
}

void switchPreviewScene(const OBSWeakSource &ws)
{
	auto source = obs_weak_source_get_source(ws);
	obs_frontend_set_current_preview_scene(source);
	obs_source_release(source);
}

void SwitcherData::Start()
{
	if (!(th && th->isRunning())) {
		stop = false;
		th = new SwitcherThread();
		th->start((QThread::Priority)threadPriority);

		// Will be overwritten quickly but might be useful
		writeToStatusFile("Advanced Scene Switcher running");
	}

	if (networkConfig.ServerEnabled) {
		server.start(networkConfig.ServerPort,
			     networkConfig.LockToIPv4);
	}

	if (networkConfig.ClientEnabled) {
		client.connect(networkConfig.GetClientUri());
	}

	if (showSystemTrayNotifications) {
		DisplayTrayMessage(
			obs_module_text("AdvSceneSwitcher.pluginName"),
			obs_module_text("AdvSceneSwitcher.running"));
	}
}

void ResetMacroCounters()
{
	for (auto &m : switcher->macros) {
		m->ResetCount();
	}
}

void SwitcherData::Stop()
{
	if (th && th->isRunning()) {
		stop = true;
		cv.notify_all();
		abortMacroWait = true;
		macroWaitCv.notify_all();
		th->wait();
		delete th;
		th = nullptr;

		writeToStatusFile("Advanced Scene Switcher stopped");
		ResetMacroCounters();
	}

	server.stop();
	client.disconnect();

	for (auto &t : audioHelperThreads) {
		if (t.joinable()) {
			t.join();
		}
	}
	audioHelperThreads.clear();

	if (showSystemTrayNotifications) {
		DisplayTrayMessage(
			obs_module_text("AdvSceneSwitcher.pluginName"),
			obs_module_text("AdvSceneSwitcher.stopped"));
	}
}

void SwitcherData::setWaitScene()
{
	waitScene = obs_frontend_get_current_scene();
	obs_source_release(waitScene);
}

bool SwitcherData::sceneChangedDuringWait()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	if (!currentSource) {
		return true;
	}
	obs_source_release(currentSource);
	return (waitScene && currentSource != waitScene);
}

/******************************************************************************
 * OBS module setup
 ******************************************************************************/
extern "C" void FreeSceneSwitcher()
{
	if (loaded_curl_lib) {
		if (switcher->curl && f_curl_cleanup) {
			f_curl_cleanup(switcher->curl);
		}
		delete loaded_curl_lib;
		loaded_curl_lib = nullptr;
	}

	PlatformCleanup();

	delete switcher;
	switcher = nullptr;
}

void handleSceneChange()
{
	switcher->lastSceneChangeTime =
		std::chrono::high_resolution_clock::now();

	// Stop waiting if scene was changed
	if (switcher->sceneChangedDuringWait()) {
		switcher->cv.notify_one();
	}

	// Set current and previous scene
	obs_source_t *source = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(source);

	if (ws && ws != switcher->currentScene) {
		switcher->previousScene = switcher->currentScene;
		switcher->currentScene = ws;
		vblog(LOG_INFO, "current scene:  %s",
		      GetWeakSourceName(switcher->currentScene).c_str());
		vblog(LOG_INFO, "previous scene: %s",
		      GetWeakSourceName(switcher->previousScene).c_str());
	}

	obs_source_release(source);
	obs_weak_source_release(ws);

	switcher->checkTriggers();
	switcher->checkDefaultSceneTransitions();

	if (switcher->networkConfig.ShouldSendFrontendSceneChange()) {
		switcher->server.sendMessage({ws, nullptr, 0});
	}
}

void setLiveTime()
{
	switcher->liveTime = QDateTime::currentDateTime();
}

void resetLiveTime()
{
	switcher->liveTime = QDateTime();
}

void checkAutoStartRecording()
{
	if (switcher->autoStartEvent == AutoStartEvent::RECORDING ||
	    switcher->autoStartEvent == AutoStartEvent::RECORINDG_OR_STREAMING)
		switcher->Start();
}

void checkAutoStartStreaming()
{
	if (switcher->autoStartEvent == AutoStartEvent::STREAMING ||
	    switcher->autoStartEvent == AutoStartEvent::RECORINDG_OR_STREAMING)
		switcher->Start();
}

void handlePeviewSceneChange()
{
	if (switcher->networkConfig.ShouldSendPrviewSceneChange()) {
		auto source = obs_frontend_get_current_preview_scene();
		auto weak = obs_source_get_weak_source(source);
		switcher->server.sendMessage({weak, nullptr, 0}, true);
		obs_weak_source_release(weak);
		obs_source_release(source);
	}
}

void setReplayBufferSaved()
{
	switcher->replayBufferSaved = true;
}

void setTranstionEnd()
{
	switcher->lastTransitionEndTime =
		std::chrono::high_resolution_clock::now();
}

void setStreamStarting()
{
	switcher->lastStreamStartingTime =
		std::chrono::high_resolution_clock::now();
}

void setStreamStopping()
{
	switcher->lastStreamStoppingTime =
		std::chrono::high_resolution_clock::now();
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
	case OBS_FRONTEND_EVENT_EXIT:
		FreeSceneSwitcher();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		handleSceneChange();
		break;
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
		handlePeviewSceneChange();
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
#ifdef REPLAYBUFFER_SUPPORTED
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		setReplayBufferSaved();
		break;
#endif
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		setTranstionEnd();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		setStreamStarting();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		setStreamStopping();
		break;
	default:
		break;
	}
}

AdvSceneSwitcher *ssWindow;

extern "C" void InitSceneSwitcher()
{
	blog(LOG_INFO, "version: %s", g_GIT_TAG);
	blog(LOG_INFO, "version: %s", g_GIT_SHA1);

	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("AdvSceneSwitcher.pluginName"));

	switcher = new SwitcherData;

	if (loadCurl() && f_curl_init) {
		switcher->curl = f_curl_init();
	}

	PlatformInit();

	auto cb = []() {
		if (switcher->settingsWindowOpened) {
			ssWindow->show();
			ssWindow->raise();
			ssWindow->activateWindow();
		} else {
			ssWindow = new AdvSceneSwitcher(
				(QMainWindow *)obs_frontend_get_main_window());
			ssWindow->setAttribute(Qt::WA_DeleteOnClose);
			ssWindow->show();
		}
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, switcher);

	action->connect(action, &QAction::triggered, cb);
}
