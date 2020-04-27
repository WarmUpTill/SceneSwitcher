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

	lock_guard<mutex> lock(switcher->m);

	switcher->Prune();

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
		temp++;
	}

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		QComboBox *combo = reinterpret_cast<QComboBox *>(data);
		if (obs_source_media_get_state(source) !=
		    OBS_MEDIA_STATE_NONE) {
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

	vector<string> windows;
	GetWindowList(windows);

	for (string &window : windows) {
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
		string sceneName = GetWeakSourceName(s.mScene);
		string transitionName = GetWeakSourceName(s.mTransition);
		QString text = MakeSwitchNameExecutable(sceneName.c_str(),
							s.mExe,
							transitionName.c_str(),
							s.mInFocus);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, s.mExe);
	}

	for (auto &s : switcher->windowSwitches) {
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString text =
			MakeSwitchName(sceneName.c_str(), s.window.c_str(),
				       transitionName.c_str(), s.fullscreen);

		QListWidgetItem *item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, s.window.c_str());
	}

	for (auto &s : switcher->screenRegionSwitches) {
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
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

	for (auto &scene : switcher->pauseScenesSwitches) {
		string sceneName = GetWeakSourceName(scene);
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
		string sceneName1 = GetWeakSourceName(s.scene1);
		string sceneName2 = (s.usePreviousScene)
					    ? PREVIOUS_SCENE_NAME
					    : GetWeakSourceName(s.scene2);
		string transitionName = GetWeakSourceName(s.transition);
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
		string sceneName1 = GetWeakSourceName(s.scene1);
		string sceneName2 = GetWeakSourceName(s.scene2);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneTransitionName(sceneName1.c_str(),
						       sceneName2.c_str(),
						       transitionName.c_str());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, text);
	}
	//(transitionDurationLongerThanInterval(switcher->interval)) ? ui->transitionWarning->setVisible(true) : ui->transitionWarning->setVisible(false);

	for (auto &s : switcher->defaultSceneTransitions) {
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
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
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeRandomSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.delay);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->randomScenesList);
		item->setData(Qt::UserRole, text);
	}

	for (auto &s : switcher->fileSwitches) {
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeFileSwitchName(
			sceneName.c_str(), transitionName.c_str(),
			s.file.c_str(), s.text.c_str(), s.useRegex, s.useTime);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->fileScenesList);
		item->setData(Qt::UserRole, listText);
	}

	for (auto &s : switcher->mediaSwitches) {
		string sourceName = GetWeakSourceName(s.source);
		string sceneName = (s.usePreviousScene)
					   ? PREVIOUS_SCENE_NAME
					   : GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeMediaSwitchName(
			sourceName.c_str(), sceneName.c_str(),
			transitionName.c_str(), s.state, s.restriction, s.time);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->mediaSwitches);
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

	if (switcher->th.joinable())
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
		string s = "";
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
		}
		QString text(s.c_str());
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->priorityList);
		item->setData(Qt::UserRole, text);
	}
}

/********************************************************************************
 * Saving and loading
 ********************************************************************************/
static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		lock_guard<mutex> lock(switcher->m);
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *array = obs_data_array_create();
		obs_data_array_t *screenRegionArray = obs_data_array_create();
		obs_data_array_t *pauseScenesArray = obs_data_array_create();
		obs_data_array_t *pauseWindowsArray = obs_data_array_create();
		obs_data_array_t *ignoreWindowsArray = obs_data_array_create();
		obs_data_array_t *sceneRoundTripArray = obs_data_array_create();
		obs_data_array_t *sceneTransitionsArray =
			obs_data_array_create();
		obs_data_array_t *defaultTransitionsArray =
			obs_data_array_create();
		obs_data_array_t *ignoreIdleWindowsArray =
			obs_data_array_create();
		obs_data_array_t *executableArray = obs_data_array_create();
		obs_data_array_t *randomArray = obs_data_array_create();
		obs_data_array_t *fileArray = obs_data_array_create();
		obs_data_array_t *mediaArray = obs_data_array_create();

		switcher->Prune();

		for (WindowSceneSwitch &s : switcher->windowSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_string(array_obj, "window_title",
						    s.window.c_str());
				obs_data_set_bool(array_obj, "fullscreen",
						  s.fullscreen);
				obs_data_array_push_back(array, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (ScreenRegionSwitch &s : switcher->screenRegionSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj,
						    "screenRegionScene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_int(array_obj, "minX", s.minX);
				obs_data_set_int(array_obj, "minY", s.minY);
				obs_data_set_int(array_obj, "maxX", s.maxX);
				obs_data_set_int(array_obj, "maxY", s.maxY);
				obs_data_set_string(array_obj,
						    "screenRegionStr",
						    s.regionStr.c_str());
				obs_data_array_push_back(screenRegionArray,
							 array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (OBSWeakSource &scene : switcher->pauseScenesSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(scene);
			if (source) {
				const char *n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "pauseScene", n);
				obs_data_array_push_back(pauseScenesArray,
							 array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		for (string &window : switcher->pauseWindowsSwitches) {
			obs_data_t *array_obj = obs_data_create();
			obs_data_set_string(array_obj, "pauseWindow",
					    window.c_str());
			obs_data_array_push_back(pauseWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (string &window : switcher->ignoreWindowsSwitches) {
			obs_data_t *array_obj = obs_data_create();
			obs_data_set_string(array_obj, "ignoreWindow",
					    window.c_str());
			obs_data_array_push_back(ignoreWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (SceneRoundTripSwitch &s :
		     switcher->sceneRoundTripSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source1 =
				obs_weak_source_get_source(s.scene1);
			obs_source_t *source2 =
				obs_weak_source_get_source(s.scene2);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if (source1 && (s.usePreviousScene || source2) &&
			    transition) {
				const char *sceneName1 =
					obs_source_get_name(source1);
				const char *sceneName2 =
					obs_source_get_name(source2);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj,
						    "sceneRoundTripScene1",
						    sceneName1);
				obs_data_set_string(
					array_obj, "sceneRoundTripScene2",
					s.usePreviousScene ? PREVIOUS_SCENE_NAME
							   : sceneName2);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_int(
					array_obj, "sceneRoundTripDelay",
					s.delay /
						1000); //delay stored in two separate values
				obs_data_set_int(
					array_obj, "sceneRoundTripDelayMs",
					s.delay %
						1000); //to be compatible with older versions
				obs_data_set_string(
					array_obj, "sceneRoundTripStr",
					s.sceneRoundTripStr.c_str());
				obs_data_array_push_back(sceneRoundTripArray,
							 array_obj);
				obs_source_release(source1);
				obs_source_release(source2);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (SceneTransition &s : switcher->sceneTransitions) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source1 =
				obs_weak_source_get_source(s.scene1);
			obs_source_t *source2 =
				obs_weak_source_get_source(s.scene2);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if (source1 && source2 && transition) {
				const char *sceneName1 =
					obs_source_get_name(source1);
				const char *sceneName2 =
					obs_source_get_name(source2);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "Scene1",
						    sceneName1);
				obs_data_set_string(array_obj, "Scene2",
						    sceneName2);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_string(
					array_obj, "Str",
					s.sceneTransitionStr.c_str());
				obs_data_array_push_back(sceneTransitionsArray,
							 array_obj);
				obs_source_release(source1);
				obs_source_release(source2);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (DefaultSceneTransition &s :
		     switcher->defaultSceneTransitions) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "Scene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_string(
					array_obj, "Str",
					s.sceneTransitionStr.c_str());
				obs_data_array_push_back(
					defaultTransitionsArray, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (ExecutableSceneSwitch &s : switcher->executableSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.mScene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.mTransition);

			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_string(array_obj, "exefile",
						    s.mExe.toUtf8());
				obs_data_set_bool(array_obj, "infocus",
						  s.mInFocus);
				obs_data_array_push_back(executableArray,
							 array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (RandomSwitch &s : switcher->randomSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);

			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_double(array_obj, "delay",
						    s.delay);
				obs_data_set_string(array_obj, "str",
						    s.randomSwitchStr.c_str());
				obs_data_array_push_back(randomArray,
							 array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (string &window : switcher->ignoreIdleWindows) {
			obs_data_t *array_obj = obs_data_create();
			obs_data_set_string(array_obj, "window",
					    window.c_str());
			obs_data_array_push_back(ignoreIdleWindowsArray,
						 array_obj);
			obs_data_release(array_obj);
		}

		for (FileSwitch &s : switcher->fileSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);

			if (source && transition) {
				const char *sceneName =
					obs_source_get_name(source);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene",
						    sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_string(array_obj, "file",
						    s.file.c_str());
				obs_data_set_string(array_obj, "text",
						    s.text.c_str());
				obs_data_set_bool(array_obj, "useRegex",
						  s.useRegex);
				obs_data_set_bool(array_obj, "useTime",
						  s.useTime);
				obs_data_array_push_back(fileArray, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (MediaSwitch &s : switcher->mediaSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source =
				obs_weak_source_get_source(s.source);
			obs_source_t *sceneSource =
				obs_weak_source_get_source(s.scene);
			obs_source_t *transition =
				obs_weak_source_get_source(s.transition);
			if ((s.usePreviousScene || sceneSource) && source &&
			    transition) {
				const char *sourceName =
					obs_source_get_name(source);
				const char *sceneName =
					obs_source_get_name(sceneSource);
				const char *transitionName =
					obs_source_get_name(transition);
				obs_data_set_string(array_obj, "source",
						    sourceName);
				obs_data_set_string(
					array_obj, "scene",
					s.usePreviousScene ? PREVIOUS_SCENE_NAME
							   : sceneName);
				obs_data_set_string(array_obj, "transition",
						    transitionName);
				obs_data_set_int(array_obj, "state", s.state);
				obs_data_set_int(array_obj, "restriction",
						 s.restriction);
				obs_data_set_int(array_obj, "time", s.time);
				obs_data_array_push_back(mediaArray, array_obj);
			}
			obs_source_release(source);
			obs_source_release(sceneSource);
			obs_source_release(transition);

			obs_data_release(array_obj);
		}

		string nonMatchingSceneName =
			GetWeakSourceName(switcher->nonMatchingScene);

		obs_data_set_int(obj, "interval", switcher->interval);
		obs_data_set_string(obj, "non_matching_scene",
				    nonMatchingSceneName.c_str());
		obs_data_set_int(obj, "switch_if_not_matching",
				 switcher->switchIfNotMatching);
		obs_data_set_bool(obj, "active", !switcher->stop);

		obs_data_set_array(obj, "switches", array);
		obs_data_set_array(obj, "screenRegion", screenRegionArray);
		obs_data_set_array(obj, "pauseScenes", pauseScenesArray);
		obs_data_set_array(obj, "pauseWindows", pauseWindowsArray);
		obs_data_set_array(obj, "ignoreWindows", ignoreWindowsArray);
		obs_data_set_array(obj, "sceneRoundTrip", sceneRoundTripArray);
		obs_data_set_array(obj, "sceneTransitions",
				   sceneTransitionsArray);
		obs_data_set_array(obj, "defaultTransitions",
				   defaultTransitionsArray);
		obs_data_set_array(obj, "executableSwitches", executableArray);
		obs_data_set_array(obj, "ignoreIdleWindows",
				   ignoreIdleWindowsArray);
		obs_data_set_array(obj, "randomSwitches", randomArray);
		obs_data_set_array(obj, "fileSwitches", fileArray);
		obs_data_set_array(obj, "mediaSwitches", mediaArray);

		string autoStopSceneName =
			GetWeakSourceName(switcher->autoStopScene);
		obs_data_set_bool(obj, "autoStopEnable",
				  switcher->autoStopEnable);
		obs_data_set_string(obj, "autoStopSceneName",
				    autoStopSceneName.c_str());

		string idleSceneName =
			GetWeakSourceName(switcher->idleData.scene);
		string idleTransitionName =
			GetWeakSourceName(switcher->idleData.transition);
		obs_data_set_bool(obj, "idleEnable",
				  switcher->idleData.idleEnable);
		obs_data_set_string(obj, "idleSceneName",
				    switcher->idleData.usePreviousScene
					    ? PREVIOUS_SCENE_NAME
					    : idleSceneName.c_str());
		obs_data_set_string(obj, "idleTransitionName",
				    idleTransitionName.c_str());
		obs_data_set_int(obj, "idleTime", switcher->idleData.time);

		obs_data_set_bool(obj, "readEnabled",
				  switcher->fileIO.readEnabled);
		obs_data_set_string(obj, "readPath",
				    switcher->fileIO.readPath.c_str());
		obs_data_set_bool(obj, "writeEnabled",
				  switcher->fileIO.writeEnabled);
		obs_data_set_string(obj, "writePath",
				    switcher->fileIO.writePath.c_str());

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

		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);

		obs_data_array_release(array);
		obs_data_array_release(screenRegionArray);
		obs_data_array_release(pauseScenesArray);
		obs_data_array_release(pauseWindowsArray);
		obs_data_array_release(ignoreWindowsArray);
		obs_data_array_release(sceneRoundTripArray);
		obs_data_array_release(sceneTransitionsArray);
		obs_data_array_release(defaultTransitionsArray);
		obs_data_array_release(executableArray);
		obs_data_array_release(ignoreIdleWindowsArray);
		obs_data_array_release(randomArray);
		obs_data_array_release(fileArray);
		obs_data_array_release(mediaArray);

		obs_data_release(obj);
	} else {
		switcher->m.lock();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "advanced-scene-switcher");
		obs_data_array_t *array = obs_data_get_array(obj, "switches");
		obs_data_array_t *screenRegionArray =
			obs_data_get_array(obj, "screenRegion");
		obs_data_array_t *pauseScenesArray =
			obs_data_get_array(obj, "pauseScenes");
		obs_data_array_t *pauseWindowsArray =
			obs_data_get_array(obj, "pauseWindows");
		obs_data_array_t *ignoreWindowsArray =
			obs_data_get_array(obj, "ignoreWindows");
		obs_data_array_t *sceneRoundTripArray =
			obs_data_get_array(obj, "sceneRoundTrip");
		obs_data_array_t *sceneTransitionsArray =
			obs_data_get_array(obj, "sceneTransitions");
		obs_data_array_t *defaultTransitionsArray =
			obs_data_get_array(obj, "defaultTransitions");
		obs_data_array_t *executableArray =
			obs_data_get_array(obj, "executableSwitches");
		obs_data_array_t *ignoreIdleWindowsArray =
			obs_data_get_array(obj, "ignoreIdleWindows");
		obs_data_array_t *randomArray =
			obs_data_get_array(obj, "randomSwitches");
		obs_data_array_t *fileArray =
			obs_data_get_array(obj, "fileSwitches");
		obs_data_array_t *mediaArray =
			obs_data_get_array(obj, "mediaSwitches");

		if (!obj)
			obj = obs_data_create();

		obs_data_set_default_int(obj, "interval", DEFAULT_INTERVAL);

		switcher->interval = obs_data_get_int(obj, "interval");
		obs_data_set_default_int(obj, "switch_if_not_matching",
					 NO_SWITCH);
		switcher->switchIfNotMatching = (NoMatch)obs_data_get_int(
			obj, "switch_if_not_matching");
		string nonMatchingScene =
			obs_data_get_string(obj, "non_matching_scene");
		bool active = obs_data_get_bool(obj, "active");

		switcher->nonMatchingScene =
			GetWeakSourceByName(nonMatchingScene.c_str());

		switcher->windowSwitches.clear();
		size_t count = obs_data_array_count(array);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(array, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			const char *window =
				obs_data_get_string(array_obj, "window_title");
			bool fullscreen =
				obs_data_get_bool(array_obj, "fullscreen");

			switcher->windowSwitches.emplace_back(
				GetWeakSourceByName(scene), window,
				GetWeakTransitionByName(transition),
				fullscreen);

			obs_data_release(array_obj);
		}

		switcher->screenRegionSwitches.clear();
		count = obs_data_array_count(screenRegionArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(screenRegionArray, i);

			const char *scene = obs_data_get_string(
				array_obj, "screenRegionScene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			int minX = obs_data_get_int(array_obj, "minX");
			int minY = obs_data_get_int(array_obj, "minY");
			int maxX = obs_data_get_int(array_obj, "maxX");
			int maxY = obs_data_get_int(array_obj, "maxY");
			string regionStr = obs_data_get_string(
				array_obj, "screenRegionStr");

			switcher->screenRegionSwitches.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition), minX, minY,
				maxX, maxY, regionStr);

			obs_data_release(array_obj);
		}

		switcher->pauseScenesSwitches.clear();
		count = obs_data_array_count(pauseScenesArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(pauseScenesArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "pauseScene");

			switcher->pauseScenesSwitches.emplace_back(
				GetWeakSourceByName(scene));

			obs_data_release(array_obj);
		}

		switcher->pauseWindowsSwitches.clear();
		count = obs_data_array_count(pauseWindowsArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(pauseWindowsArray, i);

			const char *window =
				obs_data_get_string(array_obj, "pauseWindow");

			switcher->pauseWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		obs_data_array_release(pauseWindowsArray);

		switcher->ignoreWindowsSwitches.clear();
		count = obs_data_array_count(ignoreWindowsArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(ignoreWindowsArray, i);

			const char *window =
				obs_data_get_string(array_obj, "ignoreWindow");

			switcher->ignoreWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		switcher->sceneRoundTripSwitches.clear();
		count = obs_data_array_count(sceneRoundTripArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(sceneRoundTripArray, i);

			const char *scene1 = obs_data_get_string(
				array_obj, "sceneRoundTripScene1");
			const char *scene2 = obs_data_get_string(
				array_obj, "sceneRoundTripScene2");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			int delay = obs_data_get_int(
				array_obj,
				"sceneRoundTripDelay"); //delay stored in two separate values
			delay = delay * 1000 +
				obs_data_get_int(
					array_obj,
					"sceneRoundTripDelayMs"); //to be compatible with older versions
			string str = MakeSceneRoundTripSwitchName(
					     scene1, scene2, transition,
					     ((double)delay) / 1000.0)
					     .toUtf8()
					     .constData();
			const char *sceneRoundTripStr = str.c_str();

			switcher->sceneRoundTripSwitches.emplace_back(
				GetWeakSourceByName(scene1),
				GetWeakSourceByName(scene2),
				GetWeakTransitionByName(transition), delay,
				(strcmp(scene2, PREVIOUS_SCENE_NAME) == 0),
				sceneRoundTripStr);

			obs_data_release(array_obj);
		}

		switcher->sceneTransitions.clear();
		count = obs_data_array_count(sceneTransitionsArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(sceneTransitionsArray, i);

			const char *scene1 =
				obs_data_get_string(array_obj, "Scene1");
			const char *scene2 =
				obs_data_get_string(array_obj, "Scene2");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			const char *sceneTransitionsStr =
				obs_data_get_string(array_obj, "Str");

			switcher->sceneTransitions.emplace_back(
				GetWeakSourceByName(scene1),
				GetWeakSourceByName(scene2),
				GetWeakTransitionByName(transition),
				sceneTransitionsStr);

			obs_data_release(array_obj);
		}

		switcher->defaultSceneTransitions.clear();
		count = obs_data_array_count(defaultTransitionsArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(defaultTransitionsArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "Scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			const char *sceneTransitionsStr =
				obs_data_get_string(array_obj, "Str");

			switcher->defaultSceneTransitions.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition),
				sceneTransitionsStr);

			obs_data_release(array_obj);
		}

		switcher->executableSwitches.clear();
		count = obs_data_array_count(executableArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(executableArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			const char *exe =
				obs_data_get_string(array_obj, "exefile");
			bool infocus = obs_data_get_bool(array_obj, "infocus");

			switcher->executableSwitches.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition), exe,
				infocus);

			obs_data_release(array_obj);
		}

		switcher->ignoreIdleWindows.clear();
		count = obs_data_array_count(ignoreIdleWindowsArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(ignoreIdleWindowsArray, i);

			const char *window =
				obs_data_get_string(array_obj, "window");

			switcher->ignoreIdleWindows.emplace_back(window);

			obs_data_release(array_obj);
		}

		switcher->randomSwitches.clear();
		count = obs_data_array_count(randomArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(randomArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			double delay = obs_data_get_double(array_obj, "delay");
			const char *str = obs_data_get_string(array_obj, "str");

			switcher->randomSwitches.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition), delay,
				str);

			obs_data_release(array_obj);
		}

		switcher->fileSwitches.clear();
		count = obs_data_array_count(fileArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(fileArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			const char *file =
				obs_data_get_string(array_obj, "file");
			const char *text =
				obs_data_get_string(array_obj, "text");
			bool useRegex =
				obs_data_get_bool(array_obj, "useRegex");
			bool useTime = obs_data_get_bool(array_obj, "useTime");

			switcher->fileSwitches.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition), file, text,
				useRegex, useTime);

			obs_data_release(array_obj);
		}

		switcher->mediaSwitches.clear();
		count = obs_data_array_count(mediaArray);

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj =
				obs_data_array_item(mediaArray, i);

			const char *source =
				obs_data_get_string(array_obj, "source");
			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *transition =
				obs_data_get_string(array_obj, "transition");
			obs_media_state state =
				(obs_media_state)obs_data_get_int(array_obj,
								  "state");
			time_restriction restriction =
				(time_restriction)obs_data_get_int(
					array_obj, "restriction");
			uint64_t time = obs_data_get_int(array_obj, "time");

			switcher->mediaSwitches.emplace_back(
				GetWeakSourceByName(scene),
				GetWeakSourceByName(source),
				GetWeakTransitionByName(transition), state,
				restriction, time,
				(strcmp(scene, PREVIOUS_SCENE_NAME) == 0));
		}

		string autoStopScene =
			obs_data_get_string(obj, "autoStopSceneName");
		switcher->autoStopEnable =
			obs_data_get_bool(obj, "autoStopEnable");
		switcher->autoStopScene =
			GetWeakSourceByName(autoStopScene.c_str());

		string idleSceneName =
			obs_data_get_string(obj, "idleSceneName");
		string idleTransitionName =
			obs_data_get_string(obj, "idleTransitionName");
		switcher->idleData.scene =
			GetWeakSourceByName(idleSceneName.c_str());
		switcher->idleData.transition =
			GetWeakTransitionByName(idleTransitionName.c_str());
		obs_data_set_default_bool(obj, "idleEnable", false);
		switcher->idleData.idleEnable =
			obs_data_get_bool(obj, "idleEnable");
		obs_data_set_default_int(obj, "idleTime", DEFAULT_IDLE_TIME);
		switcher->idleData.time = obs_data_get_int(obj, "idleTime");
		switcher->idleData.usePreviousScene =
			(idleSceneName == PREVIOUS_SCENE_NAME);

		obs_data_set_default_bool(obj, "readEnabled", false);
		switcher->fileIO.readEnabled =
			obs_data_get_bool(obj, "readEnabled");
		switcher->fileIO.readPath =
			obs_data_get_string(obj, "readPath");
		obs_data_set_default_bool(obj, "writeEnabled", false);
		switcher->fileIO.writeEnabled =
			obs_data_get_bool(obj, "writeEnabled");
		switcher->fileIO.writePath =
			obs_data_get_string(obj, "writePath");

		obs_data_set_default_int(obj, "priority0", DEFAULT_PRIORITY_0);
		obs_data_set_default_int(obj, "priority1", DEFAULT_PRIORITY_1);
		obs_data_set_default_int(obj, "priority2", DEFAULT_PRIORITY_2);
		obs_data_set_default_int(obj, "priority3", DEFAULT_PRIORITY_3);
		obs_data_set_default_int(obj, "priority4", DEFAULT_PRIORITY_4);
		obs_data_set_default_int(obj, "priority5", DEFAULT_PRIORITY_5);
		obs_data_set_default_int(obj, "priority6", DEFAULT_PRIORITY_6);

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
		if (!switcher->prioFuncsValid()) {
			switcher->functionNamesByPriority[0] =
				(DEFAULT_PRIORITY_0);
			switcher->functionNamesByPriority[1] =
				(DEFAULT_PRIORITY_1);
			switcher->functionNamesByPriority[2] =
				(DEFAULT_PRIORITY_2);
			switcher->functionNamesByPriority[3] =
				(DEFAULT_PRIORITY_3);
			switcher->functionNamesByPriority[4] =
				(DEFAULT_PRIORITY_4);
			switcher->functionNamesByPriority[5] =
				(DEFAULT_PRIORITY_5);
			switcher->functionNamesByPriority[6] =
				(DEFAULT_PRIORITY_6);
		}

		obs_data_array_release(array);
		obs_data_array_release(screenRegionArray);
		obs_data_array_release(pauseScenesArray);
		obs_data_array_release(ignoreWindowsArray);
		obs_data_array_release(sceneRoundTripArray);
		obs_data_array_release(sceneTransitionsArray);
		obs_data_array_release(defaultTransitionsArray);
		obs_data_array_release(executableArray);
		obs_data_array_release(ignoreIdleWindowsArray);
		obs_data_array_release(randomArray);
		obs_data_array_release(fileArray);

		obs_data_release(obj);

		switcher->m.unlock();

		if (active)
			switcher->Start();
		else
			switcher->Stop();
	}
}

/********************************************************************************
 * Main switcher thread
 ********************************************************************************/
void SwitcherData::Thread()
{

	//to avoid scene duplication when rapidly switching scene collection
	this_thread::sleep_for(chrono::seconds(2));

	int sleep = 0;

	while (true) {
	startLoop:
		unique_lock<mutex> lock(m);
		bool match = false;
		OBSWeakSource scene;
		OBSWeakSource transition;
		chrono::milliseconds duration;
		if (sleep > interval)
			duration = chrono::milliseconds(sleep);
		else
			duration = chrono::milliseconds(interval);
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
endLoop:;
}

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 unique_lock<mutex> &lock)
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
	if (!th.joinable()) {
		stop = false;
		switcher->th = thread([]() { switcher->Thread(); });
	}
}

void SwitcherData::Stop()
{
	if (th.joinable()) {
		switcher->stop = true;
		transitionCv.notify_one();
		cv.notify_one();
		th.join();
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
		lock_guard<mutex> lock(s->m);
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
