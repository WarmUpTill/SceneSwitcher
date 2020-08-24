#include <QMainWindow>
#include <QDir>
#include <QAction>

#include <condition_variable>
#include <chrono>
#include <vector>
#include <mutex>
#include <thread>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>

#include "headers/switcher-data-structs.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

SwitcherData *switcher = nullptr;

/********************************************************************************
 * Create the Advanced Scene Switcher settings window
 ********************************************************************************/
SceneSwitcher::SceneSwitcher(QWidget *parent)
	: QDialog(parent), ui(new Ui_SceneSwitcher)
{
	ui->setupUi(this);

	std::lock_guard<std::mutex> lock(switcher->m);

	switcher->Prune();
	loadUI();
}

void SceneSwitcher::populateSceneSelection(QComboBox *sel, bool addPrevious)
{
	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		sel->addItem(name);
		temp++;
	}

	if (addPrevious)
		sel->addItem(PREVIOUS_SCENE_NAME);
}

void SceneSwitcher::populateTransitionSelection(QComboBox *sel)
{
	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		sel->addItem(name);
	}

	obs_frontend_source_list_free(transitions);
}

void SceneSwitcher::populateWindowSelection(QComboBox *sel)
{
	std::vector<std::string> windows;
	GetWindowList(windows);
	sort(windows.begin(), windows.end());

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}
}

void SceneSwitcher::loadUI()
{
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

	setTabOrder();

	loading = false;
}

/********************************************************************************
 * Saving and loading
 ********************************************************************************/
static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		std::lock_guard<std::mutex> lock(switcher->m);

		switcher->Prune();

		obs_data_t *obj = obs_data_create();

		switcher->saveWindowTitleSwitches(obj);
		switcher->saveScreenRegionSwitches(obj);
		switcher->savePauseSwitches(obj);
		switcher->saveSceneRoundTripSwitches(obj);
		switcher->saveSceneTransitions(obj);
		switcher->saveIdleSwitches(obj);
		switcher->saveExecutableSwitches(obj);
		switcher->saveRandomSwitches(obj);
		switcher->saveFileSwitches(obj);
		switcher->saveMediaSwitches(obj);
		switcher->saveTimeSwitches(obj);
		switcher->saveGeneralSettings(obj);

		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);

		obs_data_release(obj);
	} else {
		switcher->m.lock();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "advanced-scene-switcher");

		if (!obj)
			obj = obs_data_create();

		switcher->loadWindowTitleSwitches(obj);
		switcher->loadScreenRegionSwitches(obj);
		switcher->loadPauseSwitches(obj);
		switcher->loadSceneRoundTripSwitches(obj);
		switcher->loadSceneTransitions(obj);
		switcher->loadIdleSwitches(obj);
		switcher->loadExecutableSwitches(obj);
		switcher->loadRandomSwitches(obj);
		switcher->loadFileSwitches(obj);
		switcher->loadMediaSwitches(obj);
		switcher->loadTimeSwitches(obj);
		switcher->loadGeneralSettings(obj);

		obs_data_release(obj);

		switcher->m.unlock();

		if (switcher->stop)
			switcher->Stop();
		else
			switcher->Start();
	}
}

/********************************************************************************
 * Main switcher thread
 ********************************************************************************/
void SwitcherData::Thread()
{
	blog(LOG_INFO, "Advanced Scene Switcher started");
	//to avoid scene duplication when rapidly switching scene collection
	std::this_thread::sleep_for(std::chrono::seconds(2));

	int sleep = 0;

	while (true) {
	startLoop:
		std::unique_lock<std::mutex> lock(m);
		bool match = false;
		OBSWeakSource scene;
		OBSWeakSource transition;
		std::chrono::milliseconds duration;
		if (sleep > interval) {
			duration = std::chrono::milliseconds(sleep);
			if (verbose)
				blog(LOG_INFO,
				     "Advanced Scene Switcher sleep for %d",
				     sleep);
		} else {
			duration = std::chrono::milliseconds(interval);
			if (verbose)
				blog(LOG_INFO,
				     "Advanced Scene Switcher sleep for %d",
				     interval);
		}
		sleep = 0;
		switcher->Prune();
		writeSceneInfoToFile();
		//sleep for a bit
		cv.wait_for(lock, duration);
		if (switcher->stop) {
			break;
		}

		setDefaultSceneTransitions();

		if (autoStopEnable) {
			autoStopStreamAndRecording();
		}

		if (autoStartEnable) {
			autoStartStreamRecording();
		}

		if (checkPause()) {
			continue;
		}

		for (int switchFuncName : functionNamesByPriority) {
			switch (switchFuncName) {
			case READ_FILE_FUNC:
				checkSwitchInfoFromFile(match, scene,
							transition);
				checkFileContent(match, scene, transition);
				break;
			case IDLE_FUNC:
				checkIdleSwitch(match, scene, transition);
				break;
			case EXE_FUNC:
				checkExeSwitch(match, scene, transition);
				break;

			case SCREEN_REGION_FUNC:
				checkScreenRegionSwitch(match, scene,
							transition);
				break;
			case WINDOW_TITLE_FUNC:
				checkWindowTitleSwitch(match, scene,
						       transition);
				break;
			case ROUND_TRIP_FUNC:
				checkSceneRoundTrip(match, scene, transition,
						    lock);
				if (sceneChangedDuringWait()) //scene might have changed during the sleep
				{
					goto startLoop;
				}
				break;
			case MEDIA_FUNC:
				checkMediaSwitch(match, scene, transition);
				break;
			case TIME_FUNC:
				checkTimeSwitch(match, scene, transition);
				break;
			}

			if (switcher->stop) {
				goto endLoop;
			}
			if (match) {
				break;
			}
		}

		if (!match && switchIfNotMatching == SWITCH &&
		    nonMatchingScene) {
			match = true;
			scene = nonMatchingScene;
			transition = nullptr;
		}
		if (!match && switchIfNotMatching == RANDOM_SWITCH) {
			checkRandom(match, scene, transition, sleep);
		}
		if (match) {
			switchScene(scene, transition,
				    tansitionOverrideOverride, lock);
		}
	}
endLoop:
	blog(LOG_INFO, "Advanced Scene Switcher stopped");
}

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 bool &transitionOverrideOverride,
		 std::unique_lock<std::mutex> &lock)
{
	obs_source_t *source = obs_weak_source_get_source(scene);
	obs_source_t *currentSource = obs_frontend_get_current_scene();

	if (source && source != currentSource) {

		// this call might block when OBS_FRONTEND_EVENT_SCENE_CHANGED is active and mutex is held
		// thus unlock mutex to avoid deadlock
		lock.unlock();

		transitionData td;
		setNextTransition(scene, currentSource, transition,
				  transitionOverrideOverride, td);
		obs_frontend_set_current_scene(source);
		if (transitionOverrideOverride)
			restoreTransitionOverride(source, td);
		lock.lock();

		if (switcher->verbose)
			blog(LOG_INFO,
			     "Advanced Scene Switcher switched scene");
	}
	obs_source_release(currentSource);
	obs_source_release(source);
}

bool SwitcherData::sceneChangedDuringWait()
{
	bool r = false;
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	if (!currentSource)
		return true;
	obs_source_release(currentSource);
	if (waitScene && currentSource != waitScene)
		r = true;
	return r;
}

void SwitcherData::Start()
{
	if (!(th && th->isRunning())) {
		stop = false;
		switcher->th = new SwitcherThread();
		switcher->th->start(
			(QThread::Priority)switcher->threadPriority);
	}
}

void SwitcherData::Stop()
{
	if (th && th->isRunning()) {
		switcher->stop = true;
		transitionCv.notify_one();
		cv.notify_one();
		th->wait();
		delete th;
		th = nullptr;
	}
}

/********************************************************************************
 * OBS module setup
 ********************************************************************************/
extern "C" void FreeSceneSwitcher()
{
	delete switcher;
	switcher = nullptr;
}

void handleSceneChange(SwitcherData *s)
{
	std::lock_guard<std::mutex> lock(s->m);
	//stop waiting if scene was manually changed
	if (s->sceneChangedDuringWait())
		s->cv.notify_one();

	//set previous scene
	obs_source_t *source = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(source);
	obs_source_release(source);
	obs_weak_source_release(ws);
	if (source && s->PreviousScene2 != ws) {
		s->previousScene = s->PreviousScene2;
		s->PreviousScene2 = ws;
	}

	//reset autostart
	s->autoStartedRecently = false;
}

static void OBSEvent(enum obs_frontend_event event, void *switcher)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_EXIT:
		FreeSceneSwitcher();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		handleSceneChange((SwitcherData *)switcher);
		break;
	default:
		break;
	}
}

extern "C" void InitSceneSwitcher()
{
	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		"Advanced Scene Switcher");

	switcher = new SwitcherData;

	auto cb = []() {
		QMainWindow *window =
			(QMainWindow *)obs_frontend_get_main_window();

		SceneSwitcher ss(window);
		ss.exec();
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, switcher);

	action->connect(action, &QAction::triggered, cb);

	char *path = obs_module_config_path("");
	QDir dir(path);
	if (!dir.exists()) {
		dir.mkpath(".");
	}
	bfree(path);

	obs_hotkey_id toggleHotkeyId = obs_hotkey_register_frontend(
		"startStopToggleSwitcherHotkey",
		"Toggle Start/Stop for the Advanced Scene Switcher",
		startStopToggleHotkeyFunc, NULL);
	loadKeybinding(toggleHotkeyId, TOGGLE_HOTKEY_PATH);
	obs_hotkey_id startHotkeyId = obs_hotkey_register_frontend(
		"startSwitcherHotkey", "Start the Advanced Scene Switcher",
		startHotkeyFunc, NULL);
	loadKeybinding(startHotkeyId, START_HOTKEY_PATH);
	obs_hotkey_id stopHotkeyId = obs_hotkey_register_frontend(
		"stopSwitcherHotkey", "Stop the Advanced Scene Switcher",
		stopHotkeyFunc, NULL);
	loadKeybinding(stopHotkeyId, STOP_HOTKEY_PATH);
}
