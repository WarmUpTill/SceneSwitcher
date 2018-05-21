#pragma once
#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>
#include <fstream>
#include "utility.hpp"

#define DEFAULT_INTERVAL 300

#define DEFAULT_IDLE_TIME 60

#define READ_FILE_FUNC 0
#define ROUND_TRIP_FUNC 1
#define IDLE_FUNC 2
#define EXE_FUNC 3
#define SCREEN_REGION_FUNC 4
#define WINDOW_TITLE_FUNC 5

#define DEFAULT_PRIORITY_0 READ_FILE_FUNC
#define DEFAULT_PRIORITY_1 ROUND_TRIP_FUNC
#define DEFAULT_PRIORITY_2 IDLE_FUNC
#define DEFAULT_PRIORITY_3 EXE_FUNC
#define DEFAULT_PRIORITY_4 SCREEN_REGION_FUNC
#define DEFAULT_PRIORITY_5 WINDOW_TITLE_FUNC

using namespace std;

struct SceneSwitch
{
	OBSWeakSource scene;
	string window;
	OBSWeakSource transition;
	bool fullscreen;

	inline SceneSwitch(
		OBSWeakSource scene_, const char* window_, OBSWeakSource transition_, bool fullscreen_)
		: scene(scene_)
		, window(window_)
		, transition(transition_)
		, fullscreen(fullscreen_)
	{
	}
};

struct ExecutableSceneSwitch
{
	OBSWeakSource mScene;
	OBSWeakSource mTransition;
	QString mExe;
	bool mInFocus;

	inline ExecutableSceneSwitch(
		OBSWeakSource scene, OBSWeakSource transition, const QString& exe, bool inFocus)
		: mScene(scene)
		, mTransition(transition)
		, mExe(exe)
		, mInFocus(inFocus)
	{
	}
};

struct ScreenRegionSwitch
{
	OBSWeakSource scene;
	OBSWeakSource transition;
	int minX, minY, maxX, maxY;
	string regionStr;

	inline ScreenRegionSwitch(OBSWeakSource scene_, OBSWeakSource transition_, int minX_, int minY_,
		int maxX_, int maxY_, string regionStr_)
		: scene(scene_)
		, transition(transition_)
		, minX(minX_)
		, minY(minY_)
		, maxX(maxX_)
		, maxY(maxY_)
		, regionStr(regionStr_)
	{
	}
};

struct SceneRoundTripSwitch
{
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	int delay;
	string sceneRoundTripStr;

	inline SceneRoundTripSwitch(OBSWeakSource scene1_, OBSWeakSource scene2_,
		OBSWeakSource transition_, int delay_, string str)
		: scene1(scene1_)
		, scene2(scene2_)
		, transition(transition_)
		, delay(delay_)
		, sceneRoundTripStr(str)
	{
	}
};

struct RandomSwitch
{
	OBSWeakSource scene;
	OBSWeakSource transition;
	double delay;
	string randomSwitchStr;

	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_
		, double delay_, string str)
		: scene(scene_)
		, transition(transition_)
		, delay(delay_)
		, randomSwitchStr(str)
	{
	}
};

struct SceneTransition
{
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	string sceneTransitionStr;

	inline SceneTransition(OBSWeakSource scene1_, OBSWeakSource scene2_, OBSWeakSource transition_,
		string sceneTransitionStr_)
		: scene1(scene1_)
		, scene2(scene2_)
		, transition(transition_)
		, sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct DefaultSceneTransition
{
	OBSWeakSource scene;
	OBSWeakSource transition;
	string sceneTransitionStr;

	inline DefaultSceneTransition(OBSWeakSource scene_, OBSWeakSource transition_,
		string sceneTransitionStr_)
		: scene(scene_)
		, transition(transition_)
		, sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct FileSwitch
{
	OBSWeakSource scene;
	OBSWeakSource transition;
	string file;
	string text;

	inline FileSwitch(
		OBSWeakSource scene_, OBSWeakSource transition_, const char* file_, const char* text_)
		: scene(scene_)
		, transition(transition_)
		, file(file_)
		, text(text_)
	{
	}
};

struct FileIOData
{
	bool readEnabled = false;
	string readPath;
	bool writeEnabled = false;
	string writePath;
};

struct IdleData
{
	bool idleEnable = false;
	int time = DEFAULT_IDLE_TIME;
	OBSWeakSource scene;
	OBSWeakSource transition;
};

typedef enum {
	NO_SWITCH = 0,
	SWITCH = 1,
	RANDOM_SWITCH = 2
} NoMatch;



struct SwitcherData
{
	thread th;
	condition_variable cv;
	mutex m;
	bool transitionActive = false;
	bool waitForTransition = false;
	condition_variable transitionCv;
	bool stop = false;

	vector<SceneSwitch> switches;
	string lastTitle;
	OBSWeakSource nonMatchingScene;
	OBSWeakSource lastRandomScene;
	int interval = DEFAULT_INTERVAL;
	NoMatch switchIfNotMatching = NO_SWITCH;
	bool startAtLaunch = false;
	vector<ScreenRegionSwitch> screenRegionSwitches;
	vector<OBSWeakSource> pauseScenesSwitches;
	vector<string> pauseWindowsSwitches;
	vector<string> ignoreWindowsSwitches;
	vector<SceneRoundTripSwitch> sceneRoundTripSwitches;
	vector<RandomSwitch> randomSwitches;
	vector<FileSwitch> fileSwitches;
	bool autoStopEnable = false;
	OBSWeakSource autoStopScene;
	string waitSceneName; //indicates which scene was active when we startet waiting on something
	vector<SceneTransition> sceneTransitions;
	vector<DefaultSceneTransition> defaultSceneTransitions;
	vector<ExecutableSceneSwitch> executableSwitches;
	FileIOData fileIO;
	IdleData idleData;
	vector<string> ignoreIdleWindows;
	vector<int> functionNamesByPriority = vector<int>{
		DEFAULT_PRIORITY_0,
		DEFAULT_PRIORITY_1,
		DEFAULT_PRIORITY_2,
		DEFAULT_PRIORITY_3,
		DEFAULT_PRIORITY_4,
		DEFAULT_PRIORITY_5,
	};

	void Thread();
	void Start();
	void Stop();

	bool sceneChangedDuringWait();
	bool prioFuncsValid();
	void writeSceneInfoToFile();
	void setDefaultSceneTransitions(unique_lock<mutex>& lock);
	void autoStopStreamAndRecording();
	bool checkPause();
	void checkSceneRoundTrip(bool& match, OBSWeakSource& scene, OBSWeakSource& transition, unique_lock<mutex>& lock);
	void checkIdleSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkWindowTitleSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkExeSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkScreenRegionSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkSwitchInfoFromFile(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkFileContent(bool& match, OBSWeakSource& scene, OBSWeakSource& transition);
	void checkRandom(bool& match, OBSWeakSource& scene, OBSWeakSource& transition, int& delay);



	void Prune()
	{
		for (size_t i = 0; i < switches.size(); i++)
		{
			SceneSwitch& s = switches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				switches.erase(switches.begin() + i--);
		}

		if (nonMatchingScene && !WeakSourceValid(nonMatchingScene))
		{
			switchIfNotMatching = NO_SWITCH;
			nonMatchingScene = nullptr;
		}

		for (size_t i = 0; i < randomSwitches.size(); i++)
		{
			RandomSwitch& s = randomSwitches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				randomSwitches.erase(randomSwitches.begin() + i--);
		}

		for (size_t i = 0; i < screenRegionSwitches.size(); i++)
		{
			ScreenRegionSwitch& s = screenRegionSwitches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				screenRegionSwitches.erase(screenRegionSwitches.begin() + i--);
		}

		for (size_t i = 0; i < pauseScenesSwitches.size(); i++)
		{
			OBSWeakSource& scene = pauseScenesSwitches[i];
			if (!WeakSourceValid(scene))
				pauseScenesSwitches.erase(pauseScenesSwitches.begin() + i--);
		}

		for (size_t i = 0; i < sceneRoundTripSwitches.size(); i++)
		{
			SceneRoundTripSwitch& s = sceneRoundTripSwitches[i];
			if (!WeakSourceValid(s.scene1) || !WeakSourceValid(s.scene2)
				|| !WeakSourceValid(s.transition))
				sceneRoundTripSwitches.erase(sceneRoundTripSwitches.begin() + i--);
		}

		if (!WeakSourceValid(autoStopScene))
		{
			autoStopScene = nullptr;
			autoStopEnable = false;
		}

		for (size_t i = 0; i < sceneTransitions.size(); i++)
		{
			SceneTransition& s = sceneTransitions[i];
			if (!WeakSourceValid(s.scene1) || !WeakSourceValid(s.scene2)
				|| !WeakSourceValid(s.transition))
				sceneTransitions.erase(sceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < defaultSceneTransitions.size(); i++)
		{
			DefaultSceneTransition& s = defaultSceneTransitions[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				defaultSceneTransitions.erase(defaultSceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < executableSwitches.size(); i++)
		{
			ExecutableSceneSwitch& s = executableSwitches[i];
			if (!WeakSourceValid(s.mScene) || !WeakSourceValid(s.mTransition))
				executableSwitches.erase(executableSwitches.begin() + i--);
		}

		for (size_t i = 0; i < fileSwitches.size(); i++)
		{
			FileSwitch& s = fileSwitches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				fileSwitches.erase(fileSwitches.begin() + i--);
		}

		if (!WeakSourceValid(idleData.scene) || !WeakSourceValid(idleData.transition))
		{
			idleData.idleEnable = false;
		}
	}
	inline ~SwitcherData()
	{
		Stop();
	}
};

