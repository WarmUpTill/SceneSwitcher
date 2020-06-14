#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>

#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QTextStream>
#include <QTimer>

#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>
#include <fstream>

#include "headers/switcher-data-structs.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

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

void SceneSwitcher::loadUI()
{

	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		ui->scenes->addItem(name);
		ui->noMatchSwitchScene->addItem(name);
		ui->screenRegionScenes->addItem(name);
		ui->pauseScenesScenes->addItem(name);
		ui->sceneRoundTripScenes1->addItem(name);
		ui->sceneRoundTripScenes2->addItem(name);
		ui->autoStopScenes->addItem(name);
		ui->transitionsScene1->addItem(name);
		ui->transitionsScene2->addItem(name);
		ui->defaultTransitionsScene->addItem(name);
		ui->executableScenes->addItem(name);
		ui->idleScenes->addItem(name);
		ui->randomScenes->addItem(name);
		ui->fileScenes->addItem(name);
		ui->mediaScenes->addItem(name);
		ui->timeScenes->addItem(name);
		temp++;
	}

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		QComboBox *combo = reinterpret_cast<QComboBox *>(data);
		if (strcmp(obs_source_get_id(source), "ffmpeg_source") == 0) {
			const char *name = obs_source_get_name(source);
			combo->addItem(name);
		}
		return true;
	};

	obs_enum_sources(sourceEnum, ui->mediaSources);

	ui->mediaStates->addItem("None");
	ui->mediaStates->addItem("Playing");
	ui->mediaStates->addItem("Opening");
	ui->mediaStates->addItem("Buffering");
	ui->mediaStates->addItem("Paused");
	ui->mediaStates->addItem("Stopped");
	ui->mediaStates->addItem("Ended");
	ui->mediaStates->addItem("Error");

	ui->mediaTimeRestrictions->addItem("None");
	ui->mediaTimeRestrictions->addItem("Time shorter");
	ui->mediaTimeRestrictions->addItem("Time longer");
	ui->mediaTimeRestrictions->addItem("Time remaining shorter");
	ui->mediaTimeRestrictions->addItem("Time remaining longer");

	ui->sceneRoundTripScenes2->addItem(PREVIOUS_SCENE_NAME);
	ui->idleScenes->addItem(PREVIOUS_SCENE_NAME);
	ui->mediaScenes->addItem(PREVIOUS_SCENE_NAME);
	ui->timeScenes->addItem(PREVIOUS_SCENE_NAME);

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		ui->transitions->addItem(name);
		ui->screenRegionsTransitions->addItem(name);
		ui->sceneRoundTripTransitions->addItem(name);
		ui->transitionsTransitions->addItem(name);
		ui->defaultTransitionsTransitions->addItem(name);
		ui->executableTransitions->addItem(name);
		ui->idleTransitions->addItem(name);
		ui->randomTransitions->addItem(name);
		ui->fileTransitions->addItem(name);
		ui->mediaTransitions->addItem(name);
		ui->timeTransitions->addItem(name);
	}

	obs_frontend_source_list_free(transitions);

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

	std::vector<std::string> windows;
	GetWindowList(windows);
	sort(windows.begin(), windows.end());

	for (std::string &window : windows) {
		ui->windows->addItem(window.c_str());
		ui->ignoreWindowsWindows->addItem(window.c_str());
		ui->pauseWindowsWindows->addItem(window.c_str());
		ui->ignoreIdleWindowsWindows->addItem(window.c_str());
	}

	QStringList processes;
	GetProcessList(processes);
	for (QString &process : processes)
		ui->executable->addItem(process);

	for (auto &s : switcher->executableSwitches) {
		std::string sceneName = GetWeakSourceName(s.mScene);
		std::string transitionName = GetWeakSourceName(s.mTransition);
		QString text = MakeSwitchNameExecutable(sceneName.c_str(),
							s.mExe,
							transitionName.c_str(),
							s.mInFocus);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, s.mExe);
	}

	for (auto &s : switcher->windowSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSwitchName(sceneName.c_str(),
					      s.window.c_str(),
					      transitionName.c_str(),
					      s.fullscreen, s.focus);

		QListWidgetItem *item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, s.window.c_str());
	}

	for (auto &s : switcher->screenRegionSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeScreenRegionSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.minX,
			s.minY, s.maxX, s.maxY);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->screenRegions);
		item->setData(Qt::UserRole, s.regionStr.c_str());
	}

	ui->autoStopSceneCheckBox->setChecked(switcher->autoStopEnable);
	ui->autoStopScenes->setCurrentText(
		GetWeakSourceName(switcher->autoStopScene).c_str());

	if (ui->autoStopSceneCheckBox->checkState()) {
		ui->autoStopScenes->setDisabled(false);
	} else {
		ui->autoStopScenes->setDisabled(true);
	}

	ui->verboseLogging->setChecked(switcher->verbose);

	for (auto &scene : switcher->pauseScenesSwitches) {
		std::string sceneName = GetWeakSourceName(scene);
		QString text = QString::fromStdString(sceneName);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseScenes);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->pauseWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->ignoreWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreWindows);
		item->setData(Qt::UserRole, text);
	}

	int smallestDelay = switcher->interval;
	for (auto &s : switcher->sceneRoundTripSwitches) {
		std::string sceneName1 = GetWeakSourceName(s.scene1);
		std::string sceneName2 = (s.usePreviousScene)
						 ? PREVIOUS_SCENE_NAME
						 : GetWeakSourceName(s.scene2);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneRoundTripSwitchName(
			sceneName1.c_str(), sceneName2.c_str(),
			transitionName.c_str(), (double)s.delay / 1000);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, text);

		if (s.delay < smallestDelay)
			smallestDelay = s.delay;
	}
	(smallestDelay < switcher->interval)
		? ui->intervalWarning->setVisible(true)
		: ui->intervalWarning->setVisible(false);

	for (auto &s : switcher->sceneTransitions) {
		std::string sceneName1 = GetWeakSourceName(s.scene1);
		std::string sceneName2 = GetWeakSourceName(s.scene2);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneTransitionName(sceneName1.c_str(),
						       sceneName2.c_str(),
						       transitionName.c_str());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, text);
	}
	//(transitionDurationLongerThanInterval(switcher->interval)) ? ui->transitionWarning->setVisible(true) : ui->transitionWarning->setVisible(false);

	for (auto &s : switcher->defaultSceneTransitions) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeDefaultSceneTransitionName(
			sceneName.c_str(), transitionName.c_str());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->defaultTransitions);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->ignoreIdleWindows) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto &s : switcher->randomSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeRandomSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.delay);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->randomScenesList);
		item->setData(Qt::UserRole, text);
	}

	for (auto &s : switcher->fileSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeFileSwitchName(
			sceneName.c_str(), transitionName.c_str(),
			s.file.c_str(), s.text.c_str(), s.useRegex, s.useTime);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->fileScenesList);
		item->setData(Qt::UserRole, listText);
	}

	for (auto &s : switcher->mediaSwitches) {
		std::string sourceName = GetWeakSourceName(s.source);
		std::string sceneName = (s.usePreviousScene)
						? PREVIOUS_SCENE_NAME
						: GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeMediaSwitchName(
			sourceName.c_str(), sceneName.c_str(),
			transitionName.c_str(), s.state, s.restriction, s.time);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->mediaSwitches);
		item->setData(Qt::UserRole, listText);
	}

	for (auto &s : switcher->timeSwitches) {
		std::string sceneName = (s.usePreviousScene)
						? PREVIOUS_SCENE_NAME
						: GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeTimeSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.time);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->timeSwitches);
		item->setData(Qt::UserRole, listText);
	}

	ui->idleCheckBox->setChecked(switcher->idleData.idleEnable);
	ui->idleScenes->setCurrentText(
		switcher->idleData.usePreviousScene
			? PREVIOUS_SCENE_NAME
			: GetWeakSourceName(switcher->idleData.scene).c_str());
	ui->idleTransitions->setCurrentText(
		GetWeakSourceName(switcher->idleData.transition).c_str());
	ui->idleSpinBox->setValue(switcher->idleData.time);

	if (ui->idleCheckBox->checkState()) {
		ui->idleScenes->setDisabled(false);
		ui->idleSpinBox->setDisabled(false);
		ui->idleTransitions->setDisabled(false);
	} else {
		ui->idleScenes->setDisabled(true);
		ui->idleSpinBox->setDisabled(true);
		ui->idleTransitions->setDisabled(true);
	}

	ui->readPathLineEdit->setText(
		QString::fromStdString(switcher->fileIO.readPath.c_str()));
	ui->readFileCheckBox->setChecked(switcher->fileIO.readEnabled);
	ui->writePathLineEdit->setText(
		QString::fromStdString(switcher->fileIO.writePath.c_str()));

	if (ui->readFileCheckBox->checkState()) {
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
	} else {
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
	}

	if (switcher->th && switcher->th->isRunning())
		SetStarted();
	else
		SetStopped();

	loading = false;

	// screen region cursor position
	QTimer *screenRegionTimer = new QTimer(this);
	connect(screenRegionTimer, SIGNAL(timeout()), this,
		SLOT(updateScreenRegionCursorPos()));
	screenRegionTimer->start(1000);

	for (int p : switcher->functionNamesByPriority) {
		std::string s = "";
		switch (p) {
		case READ_FILE_FUNC:
			s = "File Content";
			break;
		case ROUND_TRIP_FUNC:
			s = "Scene Sequence";
			break;
		case IDLE_FUNC:
			s = "Idle Detection";
			break;
		case EXE_FUNC:
			s = "Executable";
			break;
		case SCREEN_REGION_FUNC:
			s = "Screen Region";
			break;
		case WINDOW_TITLE_FUNC:
			s = "Window Title";
			break;
		case MEDIA_FUNC:
			s = "Media";
			break;
		case TIME_FUNC:
			s = "Time";
			break;
		}
		QString text(s.c_str());
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->priorityList);
		item->setData(Qt::UserRole, text);
	}

	for (int i = 0; i < switcher->threadPriorities.size(); ++i) {
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
			switchScene(scene, transition, lock);
		}
	}
endLoop:
	blog(LOG_INFO, "Advanced Scene Switcher stopped");
}

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 std::unique_lock<std::mutex> &lock)
{
	obs_source_t *source = obs_weak_source_get_source(scene);
	obs_source_t *currentSource = obs_frontend_get_current_scene();

	if (source && source != currentSource) {
		obs_weak_source_t *currentScene =
			obs_source_get_weak_source(currentSource);
		obs_weak_source_t *nextTransitionWs =
			getNextTransition(currentScene, scene);
		obs_weak_source_release(currentScene);

		if (nextTransitionWs) {
			obs_source_t *nextTransition =
				obs_weak_source_get_source(nextTransitionWs);
			lock.unlock();
			obs_frontend_set_current_transition(nextTransition);
			lock.lock();
			obs_source_release(nextTransition);
		} else if (transition) {
			obs_source_t *nextTransition =
				obs_weak_source_get_source(transition);
			lock.unlock();
			obs_frontend_set_current_transition(nextTransition);
			lock.lock();
			obs_source_release(nextTransition);
		}

		// this call might block when OBS_FRONTEND_EVENT_SCENE_CHANGED is active and mutex is held
		// thus unlock mutex to avoid deadlock
		lock.unlock();
		obs_frontend_set_current_scene(source);
		lock.lock();

		if (switcher->verbose)
			blog(LOG_INFO,
			     "Advanced Scene Switcher switched scene");

		obs_weak_source_release(nextTransitionWs);
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
	waitScene = NULL;
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

static void OBSEvent(enum obs_frontend_event event, void *switcher)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_EXIT:
		FreeSceneSwitcher();
		break;

	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		SwitcherData *s = (SwitcherData *)switcher;
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

		break;
	}
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
