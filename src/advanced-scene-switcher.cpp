#include <QMainWindow>
#include <QAction>
#include <QtGui/qstandarditemmodel.h>
#include <QPropertyAnimation>
#include <QGraphicsColorizeEffect>

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

void AdvSceneSwitcher::loadUI()
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
	setupAudioTab();

	setTabOrder();

	loading = false;
}

/********************************************************************************
 * UI helpers
 ********************************************************************************/

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
					      bool addSelect)
{
	if (addSelect)
		addSelectionEntry(sel, "--select scene--",
				  "invalid entries will not be saved");

	BPtr<char *> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		sel->addItem(name);
		temp++;
	}

	if (addPrevious)
		sel->addItem(previous_scene_name);
}

void AdvSceneSwitcher::populateTransitionSelection(QComboBox *sel,
						   bool addSelect)
{
	if (addSelect)
		addSelectionEntry(sel, "--select transition--");

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
		addSelectionEntry(sel, "--select window--");

	std::vector<std::string> windows;
	GetWindowList(windows);
	sort(windows.begin(), windows.end());

	for (std::string &window : windows) {
		sel->addItem(window.c_str());
	}

#ifdef WIN32
	sel->setItemData(
		0,
		"Use \"OBS\" to specify OBS window\nUse \"Task Switching\"to specify ALT + TAB",
		Qt::ToolTipRole);
#endif
}

void AdvSceneSwitcher::populateAudioSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(sel, "--select audio source--",
				  "invalid entries will not be saved");

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		QComboBox *combo = reinterpret_cast<QComboBox *>(data);
		uint32_t flags = obs_source_get_output_flags(source);

		if ((flags & OBS_SOURCE_AUDIO) != 0) {
			const char *name = obs_source_get_name(source);
			combo->addItem(name);
		}
		return true;
	};

	obs_enum_sources(sourceEnum, sel);
}

void AdvSceneSwitcher::populateMediaSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(sel, "--select media source--",
				  "invalid entries will not be saved");

	auto sourceEnum = [](void *data, obs_source_t *source) -> bool /* -- */
	{
		QComboBox *combo = reinterpret_cast<QComboBox *>(data);
		std::string sourceId = obs_source_get_id(source);
		if (sourceId.compare("ffmpeg_source") == 0 ||
		    sourceId.compare("vlc_source") == 0) {
			const char *name = obs_source_get_name(source);
			combo->addItem(name);
		}
		return true;
	};

	obs_enum_sources(sourceEnum, sel);
}

void AdvSceneSwitcher::populateProcessSelection(QComboBox *sel, bool addSelect)
{
	if (addSelect)
		addSelectionEntry(sel, "--select process--");

	QStringList processes;
	GetProcessList(processes);
	for (QString &process : processes)
		sel->addItem(process);
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

	while (true) {
		std::unique_lock<std::mutex> lock(m);
		bool match = false;
		OBSWeakSource scene;
		OBSWeakSource transition;
		std::chrono::milliseconds duration;
		if (sleep > interval) {
			duration = std::chrono::milliseconds(sleep);
			if (verbose)
				blog(LOG_INFO, "sleep for %d", sleep);
		} else {
			duration = std::chrono::milliseconds(interval);
			if (verbose)
				blog(LOG_INFO, "sleep for %d", interval);
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
						   lock);
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
	blog(LOG_INFO, "stopped");
}

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 bool &transitionOverrideOverride,
		 std::unique_lock<std::mutex> &lock)
{
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
	s->changedDefTransitionRecently = false;
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
		"Advanced Scene Switcher");

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
