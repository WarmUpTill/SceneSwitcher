#include <QMainWindow>
#include <QAction>
#include <QtGui/qstandarditemmodel.h>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>
#include <QMessageBox>

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/util.hpp>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/curl-helper.hpp"
#include "headers/version.h"

SwitcherData *switcher = nullptr;

/********************************************************************************
 * Create the Advanced Scene Switcher settings window
 ********************************************************************************/
AdvSceneSwitcher::AdvSceneSwitcher(QWidget *parent)
	: QDialog(parent), ui(new Ui_AdvSceneSwitcher)
{
	ui->setupUi(this);

	std::lock_guard<std::mutex> lock(switcher->m);

	switcher->Prune();
	loadUI();
}

bool translationAvailable()
{
	return !!strcmp(obs_module_text("AdvSceneSwitcher.pluginName"),
			"AdvSceneSwitcher.pluginName");
}

void AdvSceneSwitcher::loadUI()
{
	if (!translationAvailable()) {
		(void)DisplayMessage(
			"Failed to find plug-in's 'data' directory.\n"
			"Please check installation instructions!");
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
	setupSceneGroupTab();

	setTabOrder();

	loading = false;
}

/********************************************************************************
 * UI helpers
 ********************************************************************************/
bool AdvSceneSwitcher::DisplayMessage(QString msg, bool question)
{
	if (question) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			nullptr, "Advanced Scene Switcher", msg,
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			return true;
		} else {
			return false;
		}
	} else {
		QMessageBox Msgbox;
		Msgbox.setWindowTitle("Advanced Scene Switcher");
		Msgbox.setText(msg);
		Msgbox.exec();
	}

	return false;
}

void addSelectionEntry(QComboBox *sel, const char *description,
		       const char *tooltip = "")
{
	sel->addItem(description);

	if (strcmp(tooltip, "") != 0) {
		sel->setItemData(0, tooltip, Qt::ToolTipRole);
	}

	QStandardItemModel *model =
		qobject_cast<QStandardItemModel *>(sel->model());
	QModelIndex firstIndex =
		model->index(0, sel->modelColumn(), sel->rootModelIndex());
	QStandardItem *firstItem = model->itemFromIndex(firstIndex);
	firstItem->setSelectable(false);
	firstItem->setEnabled(false);
}

void AdvSceneSwitcher::populateSceneSelection(QComboBox *sel, bool addPrevious,
					      bool addSceneGroup,
					      bool addSelect, bool addIgnore)
{
	sel->clear();

	if (addSelect)
		addSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectScene"),
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));

	if (addIgnore)
		sel->addItem(obs_module_text(
			"AdvSceneSwitcher.selectIgnore"));

	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		sel->addItem(name);
		temp++;
	}

	if (addPrevious)
		sel->addItem(obs_module_text(
			"AdvSceneSwitcher.selectPreviousScene"));

	if (addSceneGroup) {
		for (auto &sg : switcher->sceneGroups) {
			sel->addItem(QString::fromStdString(sg.name));
		}
	}
}

void AdvSceneSwitcher::populateTransitionSelection(QComboBox *sel,
						   bool addSelect)
{
	if (addSelect)
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectTransition"));

	obs_frontend_source_list *transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++) {
		const char *name =
			obs_source_get_name(transitions->sources.array[i]);
		sel->addItem(name);
	}

	obs_frontend_source_list_free(transitions);
}

void AdvSceneSwitcher::populateWindowSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectWindow"));

	std::vector<std::string> windows;
	GetWindowList(windows);
	sort(windows.begin(), windows.end());

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}

#ifdef WIN32
	sel->setItemData(0, obs_module_text("AdvSceneSwitcher.selectWindowTip"),
			 Qt::ToolTipRole);
#endif
}

void AdvSceneSwitcher::populateAudioSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectAudioSource"),
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		std::vector<std::string> *list =
			reinterpret_cast<std::vector<std::string> *>(data);
		uint32_t flags = obs_source_get_output_flags(source);

		if ((flags & OBS_SOURCE_AUDIO) != 0) {
			list->push_back(obs_source_get_name(source));
		}
		return true;
	};

	std::vector<std::string> audioSources;
	obs_enum_sources(sourceEnum, &audioSources);
	sort(audioSources.begin(), audioSources.end());
	for (std::string &source : audioSources) {
		sel->addItem(source.c_str());
	}
}

void AdvSceneSwitcher::populateMediaSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(
			sel,
			obs_module_text("AdvSceneSwitcher.selectMediaSource"),
			obs_module_text(
				"AdvSceneSwitcher.invaildEntriesWillNotBeSaved"));

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		std::vector<std::string> *list =
			reinterpret_cast<std::vector<std::string> *>(data);
		std::string sourceId = obs_source_get_id(source);
		if (sourceId.compare("ffmpeg_source") == 0 ||
		    sourceId.compare("vlc_source") == 0) {
			list->push_back(obs_source_get_name(source));
		}
		return true;
	};

	std::vector<std::string> mediaSources;
	obs_enum_sources(sourceEnum, &mediaSources);
	sort(mediaSources.begin(), mediaSources.end());
	for (std::string &source : mediaSources) {
		sel->addItem(source.c_str());
	}
}

void AdvSceneSwitcher::populateProcessSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(
			sel, obs_module_text("AdvSceneSwitcher.selectProcess"));

	QStringList processes;
	GetProcessList(processes);
	processes.sort();
	for (QString &process : processes)
		sel->addItem(process);
}

void AdvSceneSwitcher::listAddClicked(QListWidget *list,
				      SwitchWidget *newWidget,
				      QPushButton *addButton,
				      QMetaObject::Connection *addHighlight)
{
	if (!list || !newWidget) {
		blog(LOG_WARNING,
		     "listAddClicked called without valid list or widget");
		return;
	}

	if (addButton && addHighlight)
		addButton->disconnect(*addHighlight);

	QListWidgetItem *item;
	item = new QListWidgetItem(list);
	list->addItem(item);
	item->setSizeHint(newWidget->minimumSizeHint());
	list->setItemWidget(item, newWidget);

	list->scrollToItem(item);
}

bool AdvSceneSwitcher::listMoveUp(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == 0)
		return false;

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index - 1, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index + 1);
	list->setCurrentRow(index - 1);
	return true;
}

bool AdvSceneSwitcher::listMoveDown(QListWidget *list)
{
	int index = list->currentRow();
	if (index == -1 || index == list->count() - 1)
		return false;

	QWidget *row = list->itemWidget(list->currentItem());
	QListWidgetItem *itemN = list->currentItem()->clone();

	list->insertItem(index + 2, itemN);
	list->setItemWidget(itemN, row);

	list->takeItem(index);
	list->setCurrentRow(index + 1);
	return true;
}

QMetaObject::Connection AdvSceneSwitcher::PulseWidget(QWidget *widget,
						      QColor endColor,
						      QColor startColor,
						      QString specifier)
{
	if (switcher->disableHints)
		return QMetaObject::Connection();

	widget->setStyleSheet(specifier + "{ \
		border-style: outset; \
		border-width: 2px; \
		border-radius: 10px; \
		border-color: rgb(0,0,0,0) \
		}");

	QGraphicsColorizeEffect *eEffect = new QGraphicsColorizeEffect(widget);
	widget->setGraphicsEffect(eEffect);
	QPropertyAnimation *paAnimation =
		new QPropertyAnimation(eEffect, "color");
	paAnimation->setStartValue(startColor);
	paAnimation->setEndValue(endColor);
	paAnimation->setDuration(1000);
	// play backward to return to original state on timer end
	paAnimation->setDirection(QAbstractAnimation::Backward);

	auto con = QWidget::connect(
		paAnimation, &QPropertyAnimation::finished, [paAnimation]() {
			QTimer::singleShot(1000, [paAnimation] {
				paAnimation->start();
			});
		});

	paAnimation->start();

	return con;
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

		switcher->saveSceneGroups(obj);
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
		switcher->saveVersion(obj, g_GIT_SHA1);

		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);

		obs_data_release(obj);
	} else {
		switcher->m.lock();

		obs_data_t *obj =
			obs_data_get_obj(save_data, "advanced-scene-switcher");

		if (!obj)
			obj = obs_data_create();
		if (switcher->versionChanged(obj, g_GIT_SHA1))
			AdvSceneSwitcher::AskBackup(obj);

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
		switcher->loadGeneralSettings(obj);
		switcher->loadHotkeys(obj);

		obs_data_release(obj);

		switcher->m.unlock();

		// stop the scene switcher at least once
		// to avoid issues with scene collection changes
		bool start = !switcher->stop;
		switcher->Stop();
		if (start)
			switcher->Start();
	}
}

/********************************************************************************
 * Main switcher thread
 ********************************************************************************/
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

		bool defTransitionMatch = false;
		OBSWeakSource defTransition;

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

		//sleep for a bit
		if (verbose)
			blog(LOG_INFO, "sleep for %ld", duration.count());
		cv.wait_for(lock, duration);

		startTime = std::chrono::high_resolution_clock::now();
		sleep = 0;
		linger = 0;

		switcher->Prune();

		if (switcher->stop) {
			break;
		}

		if (autoStopEnable) {
			autoStopStreamAndRecording();
		}

		if (autoStartEnable) {
			autoStartStreamRecording();
		}

		if (checkPause()) {
			continue;
		}

		checkDefaultSceneTransitions(defTransitionMatch, defTransition);

		for (int switchFuncName : functionNamesByPriority) {
			switch (switchFuncName) {
			case read_file_func:
				checkSwitchInfoFromFile(match, scene,
							transition);
				checkFileContent(match, scene, transition);
				break;
			case idle_func:
				checkIdleSwitch(match, scene, transition);
				break;
			case exe_func:
				checkExeSwitch(match, scene, transition);
				break;
			case screen_region_func:
				checkScreenRegionSwitch(match, scene,
							transition);
				break;
			case window_title_func:
				checkWindowTitleSwitch(match, scene,
						       transition);
				break;
			case round_trip_func:
				checkSceneSequence(match, scene, transition,
						   linger);
				break;
			case media_func:
				checkMediaSwitch(match, scene, transition);
				break;
			case time_func:
				checkTimeSwitch(match, scene, transition);
				break;
			case audio_func:
				checkAudioSwitch(match, scene, transition);
				break;
			}

			if (switcher->stop) {
				goto endLoop;
			}
			if (match) {
				break;
			}
		}

		checkNoMatchSwitch(match, scene, transition, sleep);

		checkSwitchCooldown(match);

		if (linger) {
			duration = std::chrono::milliseconds(linger);
			if (verbose)
				blog(LOG_INFO,
				     "sleep for %ld before switching scene",
				     duration.count());

			cv.wait_for(lock, duration);

			if (switcher->stop) {
				break;
			}

			if (sceneChangedDuringWait()) {
				if (verbose)
					blog(LOG_INFO,
					     "scene was changed manually - ignoring match");
				match = false;
				linger = 0;
			}
		}

		// After this point we will call frontend functions
		// obs_frontend_set_current_scene() and
		// obs_frontend_set_current_transition()
		//
		// During this time SaveSceneSwitcher() could be called
		// leading to a deadlock, so we have to unlock()
		lock.unlock();

		if (!match && defTransitionMatch) {
			setCurrentDefTransition(defTransition);
		}

		if (match) {
			switchScene(scene, transition,
				    tansitionOverrideOverride);
		}

		writeSceneInfoToFile();
	}
endLoop:
	blog(LOG_INFO, "stopped");
}

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 bool &transitionOverrideOverride)
{
	if (!scene && switcher->verbose) {
		blog(LOG_INFO, "nothing to switch to");
		return;
	}

	obs_source_t *source = obs_weak_source_get_source(scene);
	obs_source_t *currentSource = obs_frontend_get_current_scene();

	if (source && source != currentSource) {
		transitionData td;
		setNextTransition(scene, currentSource, transition,
				  transitionOverrideOverride, td);
		obs_frontend_set_current_scene(source);
		if (transitionOverrideOverride)
			restoreTransitionOverride(source, td);

		if (switcher->verbose)
			blog(LOG_INFO, "switched scene");
	}
	obs_source_release(currentSource);
	obs_source_release(source);
}

void SwitcherData::Start()
{
	if (!(th && th->isRunning())) {
		stop = false;
		switcher->th = new SwitcherThread();
		switcher->th->start(
			(QThread::Priority)switcher->threadPriority);

		// Will be overwritten quickly but might be useful
		writeToStatusFile("Advanced Scene Switcher running");
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

		writeToStatusFile("Advanced Scene Switcher stopped");
	}
}

bool SwitcherData::sceneChangedDuringWait()
{
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	if (!currentSource)
		return true;
	obs_source_release(currentSource);
	return (waitScene && currentSource != waitScene);
}

/********************************************************************************
 * OBS module setup
 ********************************************************************************/
extern "C" void FreeSceneSwitcher()
{
	if (loaded_curl_lib) {
		if (switcher->curl && f_curl_cleanup)
			f_curl_cleanup(switcher->curl);
		delete loaded_curl_lib;
		loaded_curl_lib = nullptr;
	}

	delete switcher;
	switcher = nullptr;
}

void handleSceneChange(SwitcherData *s)
{
	//stop waiting if scene was manually changed
	if (s->sceneChangedDuringWait())
		s->cv.notify_one();

	//set previous scene
	obs_source_t *source = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(source);
	obs_source_release(source);
	obs_weak_source_release(ws);
	if (source && s->previousScene2 != ws) {
		s->previousScene = s->previousScene2;
		s->previousScene2 = ws;
	}

	//reset events only hanled on scene change
	s->autoStartedRecently = false;
}

void handleTransitionStop(SwitcherData *s)
{
	s->checkedDefTransition = false;
}

void setLiveTime(SwitcherData *s)
{
	s->liveTime = QDateTime::currentDateTime();
}

void resetLiveTime(SwitcherData *s)
{
	s->liveTime = QDateTime();
}

// Note to future self:
// be careful using switcher->m here as there is potential for deadlocks when using
// frontend functions such as obs_frontend_set_current_scene()
static void OBSEvent(enum obs_frontend_event event, void *switcher)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_EXIT:
		FreeSceneSwitcher();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		handleSceneChange((SwitcherData *)switcher);
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
		handleTransitionStop((SwitcherData *)switcher);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		setLiveTime((SwitcherData *)switcher);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		resetLiveTime((SwitcherData *)switcher);
		break;
	default:
		break;
	}
}

extern "C" void InitSceneSwitcher()
{
	blog(LOG_INFO, "version: %s", g_GIT_SHA1);

	QAction *action = (QAction *)obs_frontend_add_tools_menu_qaction(
		obs_module_text("AdvSceneSwitcher.pluginName"));

	switcher = new SwitcherData;

	if (loadCurl() && f_curl_init) {
		switcher->curl = f_curl_init();
	}

	auto cb = []() {
		QMainWindow *window =
			(QMainWindow *)obs_frontend_get_main_window();

		AdvSceneSwitcher ss(window);
		ss.exec();
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, switcher);

	action->connect(action, &QAction::triggered, cb);
}
