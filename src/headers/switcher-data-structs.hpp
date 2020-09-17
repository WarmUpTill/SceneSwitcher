#pragma once
#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <regex>
#include <mutex>
#include <fstream>
#include <QDateTime>
#include <QThread>
#include <curl/curl.h>
#include "utility.hpp"

constexpr auto default_interval = 300;

constexpr auto default_idle_time = 60;

constexpr auto previous_scene_name = "Previous Scene";

constexpr auto read_file_func = 0;
constexpr auto round_trip_func = 1;
constexpr auto idle_func = 2;
constexpr auto exe_func = 3;
constexpr auto screen_region_func = 4;
constexpr auto window_title_func = 5;
constexpr auto media_func = 6;
constexpr auto time_func = 7;
constexpr auto audio_func = 8;

constexpr auto default_priority_0 = read_file_func;
constexpr auto default_priority_1 = round_trip_func;
constexpr auto default_priority_2 = idle_func;
constexpr auto default_priority_3 = exe_func;
constexpr auto default_priority_4 = screen_region_func;
constexpr auto default_priority_5 = window_title_func;
constexpr auto default_priority_6 = media_func;
constexpr auto default_priority_7 = time_func;
constexpr auto default_priority_8 = audio_func;

/********************************************************************************
 * Data structs for each scene switching method
 ********************************************************************************/
struct WindowSceneSwitch {
	OBSWeakSource scene;
	std::string window;
	OBSWeakSource transition;
	bool fullscreen;
	bool focus;

	inline WindowSceneSwitch(OBSWeakSource scene_, const char *window_,
				 OBSWeakSource transition_, bool fullscreen_,
				 bool focus_)
		: scene(scene_),
		  window(window_),
		  transition(transition_),
		  fullscreen(fullscreen_),
		  focus(focus_)
	{
	}
};

struct ExecutableSceneSwitch {
	OBSWeakSource mScene;
	OBSWeakSource mTransition;
	QString mExe;
	bool mInFocus;

	inline ExecutableSceneSwitch(OBSWeakSource scene,
				     OBSWeakSource transition,
				     const QString &exe, bool inFocus)
		: mScene(scene),
		  mTransition(transition),
		  mExe(exe),
		  mInFocus(inFocus)
	{
	}
};

struct ScreenRegionSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	int minX, minY, maxX, maxY;
	std::string regionStr;

	inline ScreenRegionSwitch(OBSWeakSource scene_,
				  OBSWeakSource transition_, int minX_,
				  int minY_, int maxX_, int maxY_,
				  std::string regionStr_)
		: scene(scene_),
		  transition(transition_),
		  minX(minX_),
		  minY(minY_),
		  maxX(maxX_),
		  maxY(maxY_),
		  regionStr(regionStr_)
	{
	}
};

struct SceneRoundTripSwitch {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	double delay;
	bool usePreviousScene;
	std::string sceneRoundTripStr;

	inline SceneRoundTripSwitch(OBSWeakSource scene1_,
				    OBSWeakSource scene2_,
				    OBSWeakSource transition_, double delay_,
				    bool usePreviousScene_, std::string str)
		: scene1(scene1_),
		  scene2(scene2_),
		  transition(transition_),
		  delay(delay_),
		  usePreviousScene(usePreviousScene_),
		  sceneRoundTripStr(str)
	{
	}
};

struct RandomSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	double delay;
	std::string randomSwitchStr;

	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			    double delay_, std::string str)
		: scene(scene_),
		  transition(transition_),
		  delay(delay_),
		  randomSwitchStr(str)
	{
	}
};

struct SceneTransition {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	std::string sceneTransitionStr;

	inline SceneTransition(OBSWeakSource scene1_, OBSWeakSource scene2_,
			       OBSWeakSource transition_,
			       std::string sceneTransitionStr_)
		: scene1(scene1_),
		  scene2(scene2_),
		  transition(transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct DefaultSceneTransition {
	OBSWeakSource scene;
	OBSWeakSource transition;
	std::string sceneTransitionStr;

	inline DefaultSceneTransition(OBSWeakSource scene_,
				      OBSWeakSource transition_,
				      std::string sceneTransitionStr_)
		: scene(scene_),
		  transition(transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct FileSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	std::string file;
	std::string text;
	bool remote = false;
	bool useRegex = false;
	bool useTime = false;
	QDateTime lastMod;

	inline FileSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  const char *file_, const char *text_, bool remote_,
			  bool useRegex_, bool useTime_)
		: scene(scene_),
		  transition(transition_),
		  file(file_),
		  text(text_),
		  remote(remote_),
		  useRegex(useRegex_),
		  useTime(useTime_),
		  lastMod()
	{
	}
};

struct FileIOData {
	bool readEnabled = false;
	std::string readPath;
	bool writeEnabled = false;
	std::string writePath;
};

struct IdleData {
	bool idleEnable = false;
	int time = default_idle_time;
	OBSWeakSource scene;
	OBSWeakSource transition;
	bool usePreviousScene;
	bool alreadySwitched = false;
};

struct MediaSwitch {
	OBSWeakSource scene;
	OBSWeakSource source;
	OBSWeakSource transition;
	obs_media_state state;
	int64_t time;
	time_restriction restriction;
	bool matched;
	bool usePreviousScene;
	std::string mediaSwitchStr;

	inline MediaSwitch(OBSWeakSource scene_, OBSWeakSource source_,
			   OBSWeakSource transition_, obs_media_state state_,
			   time_restriction restriction_, uint64_t time_,
			   bool usePreviousScene_, std::string mediaSwitchStr_)
		: scene(scene_),
		  source(source_),
		  transition(transition_),
		  state(state_),
		  restriction(restriction_),
		  time(time_),
		  usePreviousScene(usePreviousScene_),
		  mediaSwitchStr(mediaSwitchStr_)
	{
	}
};

struct TimeSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	timeTrigger trigger;
	QTime time;
	bool usePreviousScene;
	std::string timeSwitchStr;

	inline TimeSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  timeTrigger trigger_, QTime time_,
			  bool usePreviousScene_, std::string timeSwitchStr_)
		: scene(scene_),
		  transition(transition_),
		  trigger(trigger_),
		  time(time_),
		  usePreviousScene(usePreviousScene_),
		  timeSwitchStr(timeSwitchStr_)
	{
	}
};

struct AudioSwitch {
	OBSWeakSource scene;
	OBSWeakSource audioSource;
	OBSWeakSource transition;
	double volume;
	bool usePreviousScene;
	std::string audioSwitchStr;

	inline AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			   bool usePreviousScene_, std::string audioSwitchStr_)
		: scene(scene_),
		  transition(transition_),
		  usePreviousScene(usePreviousScene_),
		  audioSwitchStr(audioSwitchStr_)
	{
	}
};

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
	bool tansitionOverrideOverride = false;

	int interval = default_interval;

	obs_source_t *waitScene = NULL; //scene during which wait started
	OBSWeakSource previousScene = NULL;
	OBSWeakSource PreviousScene2 = NULL;
	OBSWeakSource lastRandomScene;
	OBSWeakSource nonMatchingScene;
	NoMatch switchIfNotMatching = NO_SWITCH;
	StartupBehavior startupBehavior = PERSIST;

	std::vector<WindowSceneSwitch> windowSwitches;
	std::vector<std::string> ignoreIdleWindows;
	std::string lastTitle;

	std::vector<ScreenRegionSwitch> screenRegionSwitches;

	std::vector<OBSWeakSource> pauseScenesSwitches;

	std::vector<std::string> pauseWindowsSwitches;

	std::vector<std::string> ignoreWindowsSwitches;

	std::vector<SceneRoundTripSwitch> sceneRoundTripSwitches;
	int sceneRoundTripUnitMultiplier = 1;

	std::vector<RandomSwitch> randomSwitches;

	FileIOData fileIO;
	IdleData idleData;
	std::vector<FileSwitch> fileSwitches;
	CURL *curl = nullptr;

	std::vector<ExecutableSceneSwitch> executableSwitches;

	bool autoStopEnable = false;
	OBSWeakSource autoStopScene;

	bool autoStartEnable = false;
	AutoStartType autoStartType = RECORDING;
	OBSWeakSource autoStartScene;
	bool autoStartedRecently = false;

	std::vector<SceneTransition> sceneTransitions;
	std::vector<DefaultSceneTransition> defaultSceneTransitions;

	std::vector<MediaSwitch> mediaSwitches;

	std::vector<TimeSwitch> timeSwitches;
	QDateTime liveTime;

	std::vector<AudioSwitch> audioSwitches;

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
	void checkSceneRoundTrip(bool &match, OBSWeakSource &scene,
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
	void saveSceneRoundTripSwitches(obs_data_t *obj);
	void saveSceneTransitions(obs_data_t *obj);
	void saveIdleSwitches(obs_data_t *obj);
	void saveExecutableSwitches(obs_data_t *obj);
	void saveRandomSwitches(obs_data_t *obj);
	void saveFileSwitches(obs_data_t *obj);
	void saveMediaSwitches(obs_data_t *obj);
	void saveTimeSwitches(obs_data_t *obj);
	void saveAudioSwitches(obs_data_t *obj);
	void saveGeneralSettings(obs_data_t *obj);

	void loadWindowTitleSwitches(obs_data_t *obj);
	void loadScreenRegionSwitches(obs_data_t *obj);
	void loadPauseSwitches(obs_data_t *obj);
	void loadSceneRoundTripSwitches(obs_data_t *obj);
	void loadSceneTransitions(obs_data_t *obj);
	void loadIdleSwitches(obs_data_t *obj);
	void loadExecutableSwitches(obs_data_t *obj);
	void loadRandomSwitches(obs_data_t *obj);
	void loadFileSwitches(obs_data_t *obj);
	void loadMediaSwitches(obs_data_t *obj);
	void loadTimeSwitches(obs_data_t *obj);
	void loadAudioSwitches(obs_data_t *obj);
	void loadGeneralSettings(obs_data_t *obj);

	void Prune()
	{
		for (size_t i = 0; i < windowSwitches.size(); i++) {
			WindowSceneSwitch &s = windowSwitches[i];
			if (!WeakSourceValid(s.scene) ||
			    !WeakSourceValid(s.transition))
				windowSwitches.erase(windowSwitches.begin() +
						     i--);
		}

		if (nonMatchingScene && !WeakSourceValid(nonMatchingScene)) {
			switchIfNotMatching = NO_SWITCH;
			nonMatchingScene = nullptr;
		}

		for (size_t i = 0; i < randomSwitches.size(); i++) {
			RandomSwitch &s = randomSwitches[i];
			if (!WeakSourceValid(s.scene) ||
			    !WeakSourceValid(s.transition))
				randomSwitches.erase(randomSwitches.begin() +
						     i--);
		}

		for (size_t i = 0; i < screenRegionSwitches.size(); i++) {
			ScreenRegionSwitch &s = screenRegionSwitches[i];
			if (!WeakSourceValid(s.scene) ||
			    !WeakSourceValid(s.transition))
				screenRegionSwitches.erase(
					screenRegionSwitches.begin() + i--);
		}

		for (size_t i = 0; i < pauseScenesSwitches.size(); i++) {
			OBSWeakSource &scene = pauseScenesSwitches[i];
			if (!WeakSourceValid(scene))
				pauseScenesSwitches.erase(
					pauseScenesSwitches.begin() + i--);
		}

		for (size_t i = 0; i < sceneRoundTripSwitches.size(); i++) {
			SceneRoundTripSwitch &s = sceneRoundTripSwitches[i];
			if (!WeakSourceValid(s.scene1) ||
			    (!s.usePreviousScene &&
			     !WeakSourceValid(s.scene2)) ||
			    !WeakSourceValid(s.transition))
				sceneRoundTripSwitches.erase(
					sceneRoundTripSwitches.begin() + i--);
		}

		if (!WeakSourceValid(autoStopScene)) {
			autoStopScene = nullptr;
			autoStopEnable = false;
		}

		for (size_t i = 0; i < sceneTransitions.size(); i++) {
			SceneTransition &s = sceneTransitions[i];
			if (!WeakSourceValid(s.scene1) ||
			    !WeakSourceValid(s.scene2) ||
			    !WeakSourceValid(s.transition))
				sceneTransitions.erase(
					sceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < defaultSceneTransitions.size(); i++) {
			DefaultSceneTransition &s = defaultSceneTransitions[i];
			if (!WeakSourceValid(s.scene) ||
			    !WeakSourceValid(s.transition))
				defaultSceneTransitions.erase(
					defaultSceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < executableSwitches.size(); i++) {
			ExecutableSceneSwitch &s = executableSwitches[i];
			if (!WeakSourceValid(s.mScene) ||
			    !WeakSourceValid(s.mTransition))
				executableSwitches.erase(
					executableSwitches.begin() + i--);
		}

		for (size_t i = 0; i < fileSwitches.size(); i++) {
			FileSwitch &s = fileSwitches[i];
			if (!WeakSourceValid(s.scene) ||
			    !WeakSourceValid(s.transition))
				fileSwitches.erase(fileSwitches.begin() + i--);
		}

		for (size_t i = 0; i < timeSwitches.size(); i++) {
			TimeSwitch &s = timeSwitches[i];
			if ((!s.usePreviousScene &&
			     !WeakSourceValid(s.scene)) ||
			    !WeakSourceValid(s.transition))
				timeSwitches.erase(timeSwitches.begin() + i--);
		}

		if (!idleData.usePreviousScene &&
			    !WeakSourceValid(idleData.scene) ||
		    !WeakSourceValid(idleData.transition)) {
			idleData.idleEnable = false;
		}

		for (size_t i = 0; i < mediaSwitches.size(); i++) {
			MediaSwitch &s = mediaSwitches[i];
			if ((!s.usePreviousScene &&
			     !WeakSourceValid(s.scene)) ||
			    !WeakSourceValid(s.source) ||
			    !WeakSourceValid(s.transition))
				mediaSwitches.erase(mediaSwitches.begin() +
						    i--);
		}

		for (size_t i = 0; i < audioSwitches.size(); i++) {
			AudioSwitch &s = audioSwitches[i];
			if ((!s.usePreviousScene &&
			     !WeakSourceValid(s.scene)) ||
			    !WeakSourceValid(s.transition))
				audioSwitches.erase(audioSwitches.begin() +
						    i--);
		}
	}
	inline ~SwitcherData() { Stop(); }
};

extern SwitcherData *switcher;
class SwitcherThread : public QThread {
public:
	explicit SwitcherThread(){};
	void run() { switcher->Thread(); };
};
