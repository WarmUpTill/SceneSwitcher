#pragma once
#include <condition_variable>
#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <QDateTime>
#include <QThread>
#include <curl/curl.h>

#include "switch-audio.hpp"
#include "switch-executable.hpp"
#include "switch-file.hpp"
#include "switch-idle.hpp"
#include "switch-media.hpp"
#include "switch-random.hpp"
#include "switch-screen-region.hpp"
#include "switch-time.hpp"
#include "switch-transitions.hpp"
#include "switch-window.hpp"
#include "swtich-sequence.hpp"

constexpr auto default_interval = 300;
constexpr auto previous_scene_name = "Previous Scene";

typedef enum { NO_SWITCH = 0, SWITCH = 1, RANDOM_SWITCH = 2 } NoMatch;
typedef enum { PERSIST = 0, START = 1, STOP = 2 } StartupBehavior;
typedef enum {
	RECORDING = 0,
	STREAMING = 1,
	RECORINDGSTREAMING = 2
} AutoStartType;

class SwitcherThread;

/********************************************************************************
 * SwitcherData
 ********************************************************************************/
struct SwitcherData {
	SwitcherThread *th = nullptr;

	std::condition_variable cv;
	std::mutex m;
	bool transitionActive = false;
	bool waitForTransition = false;
	std::condition_variable transitionCv;
	bool stop = false;
	bool verbose = false;
	bool disableHints = false;
	bool tansitionOverrideOverride = false;

	int interval = default_interval;

	obs_source_t *waitScene = NULL;
	OBSWeakSource previousScene = NULL;
	OBSWeakSource PreviousScene2 = NULL;
	OBSWeakSource lastRandomScene;
	OBSWeakSource nonMatchingScene;
	NoMatch switchIfNotMatching = NO_SWITCH;
	StartupBehavior startupBehavior = PERSIST;

	std::deque<WindowSwitch> windowSwitches;
	std::vector<std::string> ignoreIdleWindows;
	std::string lastTitle;

	std::vector<ScreenRegionSwitch> screenRegionSwitches;

	std::vector<OBSWeakSource> pauseScenesSwitches;

	std::vector<std::string> pauseWindowsSwitches;

	std::vector<std::string> ignoreWindowsSwitches;

	std::vector<SceneSequenceSwitch> sceneSequenceSwitches;
	int sceneSequenceMultiplier = 1;

	std::deque<RandomSwitch> randomSwitches;

	FileIOData fileIO;
	IdleData idleData;
	std::vector<FileSwitch> fileSwitches;
	CURL *curl = nullptr;

	std::deque<ExecutableSwitch> executableSwitches;

	bool autoStopEnable = false;
	OBSWeakSource autoStopScene;

	bool autoStartEnable = false;
	AutoStartType autoStartType = RECORDING;
	OBSWeakSource autoStartScene;
	bool autoStartedRecently = false;

	std::vector<SceneTransition> sceneTransitions;
	std::vector<DefaultSceneTransition> defaultSceneTransitions;

	std::deque<MediaSwitch> mediaSwitches;

	std::deque<TimeSwitch> timeSwitches;
	QDateTime liveTime;

	std::deque<AudioSwitch> audioSwitches;

	std::vector<int> functionNamesByPriority = std::vector<int>{
		default_priority_0, default_priority_1, default_priority_2,
		default_priority_3, default_priority_4, default_priority_5,
		default_priority_6, default_priority_7, default_priority_8};

	struct ThreadPrio {
		std::string name;
		std::string description;
		uint32_t value;
	};

	std::vector<ThreadPrio> threadPriorities{
		{"Idle",
		 "scheduled only when no other threads are running (lowest CPU load)",
		 QThread::IdlePriority},
		{"Lowest", "scheduled less often than LowPriority",
		 QThread::LowestPriority},
		{"Low", "scheduled less often than NormalPriority",
		 QThread::LowPriority},
		{"Normal", "the default priority of the operating system",
		 QThread::NormalPriority},
		{"High", "scheduled more often than NormalPriority",
		 QThread::HighPriority},
		{"Highest", "scheduled more often than HighPriority",
		 QThread::HighestPriority},
		{"Time critical",
		 "scheduled as often as possible (highest CPU load)",
		 QThread::TimeCriticalPriority},
	};

	uint32_t threadPriority = QThread::NormalPriority;

	std::vector<int> tabOrder;

	bool hotkeysRegistered = false;
	obs_hotkey_id startHotkey;
	obs_hotkey_id stopHotkey;
	obs_hotkey_id toggleHotkey;

	void Thread();
	void Start();
	void Stop();

	bool sceneChangedDuringWait();
	bool prioFuncsValid();
	void writeSceneInfoToFile();
	void setDefaultSceneTransitions();
	void autoStopStreamAndRecording();
	void autoStartStreamRecording();
	bool checkPause();
	void checkSceneSequence(bool &match, OBSWeakSource &scene,
				OBSWeakSource &transition,
				std::unique_lock<std::mutex> &lock);
	void checkIdleSwitch(bool &match, OBSWeakSource &scene,
			     OBSWeakSource &transition);
	void checkWindowTitleSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition);
	void checkExeSwitch(bool &match, OBSWeakSource &scene,
			    OBSWeakSource &transition);
	void checkScreenRegionSwitch(bool &match, OBSWeakSource &scene,
				     OBSWeakSource &transition);
	void checkSwitchInfoFromFile(bool &match, OBSWeakSource &scene,
				     OBSWeakSource &transition);
	void checkFileContent(bool &match, OBSWeakSource &scene,
			      OBSWeakSource &transition);
	void checkRandom(bool &match, OBSWeakSource &scene,
			 OBSWeakSource &transition, int &delay);
	void checkMediaSwitch(bool &match, OBSWeakSource &scene,
			      OBSWeakSource &transition);
	void checkTimeSwitch(bool &match, OBSWeakSource &scene,
			     OBSWeakSource &transition);
	void checkAudioSwitch(bool &match, OBSWeakSource &scene,
			      OBSWeakSource &transition);

	void saveWindowTitleSwitches(obs_data_t *obj);
	void saveScreenRegionSwitches(obs_data_t *obj);
	void savePauseSwitches(obs_data_t *obj);
	void saveSceneSequenceSwitches(obs_data_t *obj);
	void saveSceneTransitions(obs_data_t *obj);
	void saveIdleSwitches(obs_data_t *obj);
	void saveExecutableSwitches(obs_data_t *obj);
	void saveRandomSwitches(obs_data_t *obj);
	void saveFileSwitches(obs_data_t *obj);
	void saveMediaSwitches(obs_data_t *obj);
	void saveTimeSwitches(obs_data_t *obj);
	void saveAudioSwitches(obs_data_t *obj);
	void saveGeneralSettings(obs_data_t *obj);
	void saveHotkeys(obs_data_t *obj);

	void loadWindowTitleSwitches(obs_data_t *obj);
	void loadScreenRegionSwitches(obs_data_t *obj);
	void loadPauseSwitches(obs_data_t *obj);
	void loadSceneSequenceSwitches(obs_data_t *obj);
	void loadSceneTransitions(obs_data_t *obj);
	void loadIdleSwitches(obs_data_t *obj);
	void loadExecutableSwitches(obs_data_t *obj);
	void loadRandomSwitches(obs_data_t *obj);
	void loadFileSwitches(obs_data_t *obj);
	void loadMediaSwitches(obs_data_t *obj);
	void loadTimeSwitches(obs_data_t *obj);
	void loadAudioSwitches(obs_data_t *obj);
	void loadGeneralSettings(obs_data_t *obj);
	void loadHotkeys(obs_data_t *obj);

	void Prune();
	inline ~SwitcherData() { Stop(); }
};

extern SwitcherData *switcher;
class SwitcherThread : public QThread {
public:
	explicit SwitcherThread(){};
	void run() { switcher->Thread(); };
};
