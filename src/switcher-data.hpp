#pragma once
#include "scene-group.hpp"
#include "scene-trigger.hpp"
#include "switch-audio.hpp"
#include "switch-executable.hpp"
#include "switch-file.hpp"
#include "switch-idle.hpp"
#include "switch-media.hpp"
#include "switch-pause.hpp"
#include "switch-random.hpp"
#include "switch-screen-region.hpp"
#include "switch-time.hpp"
#include "switch-transitions.hpp"
#include "switch-window.hpp"
#include "switch-sequence.hpp"
#include "switch-video.hpp"
#include "switch-network.hpp"

#include "macro-properties.hpp"
#include "duration-control.hpp"
#include "curl-helper.hpp"
#include "priority-helper.hpp"
#include "log-helper.hpp"

#include <condition_variable>
#include <vector>
#include <deque>
#include <mutex>
#include <QDateTime>
#include <QThread>
#include <curl/curl.h>
#include <unordered_map>

namespace advss {

constexpr auto default_interval = 300;
constexpr auto tab_count = 18;

typedef const char *(*translateFunc)(const char *);

class Item;
class Macro;
class SwitcherThread;

class SwitcherData;
extern SwitcherData *switcher;
SwitcherData *GetSwitcher();
std::mutex *GetSwitcherMutex();
bool VerboseLoggingEnabled();

class SwitcherData {
public:
	void Thread();
	void Start();
	void Stop();
	std::unique_lock<std::mutex> *GetLock() { return mainLoopLock; }

	const char *Translate(const char *);
	obs_module_t *GetModule();

	void SetWaitScene();
	bool SceneChangedDuringWait();
	bool AnySceneTransitionStarted();

	void SetPreconditions();
	void ResetForNextInterval();
	void AddResetForNextIntervalFunction(std::function<void()>);
	bool CheckForMatch(OBSWeakSource &scene, OBSWeakSource &transition,
			   int &linger, bool &setPreviousSceneAsMatch,
			   bool &macroMatch);
	bool CheckMacros();
	bool RunMacros();
	void CheckNoMatchSwitch(bool &match, OBSWeakSource &scene,
				OBSWeakSource &transition, int &sleep);

	/* --- Start of saving / loading section --- */

	void SaveSettings(obs_data_t *obj);
	void SaveMacros(obs_data_t *obj);
	void SaveConnections(obs_data_t *obj);
	void SaveVariables(obs_data_t *obj);
	void SaveGeneralSettings(obs_data_t *obj);
	void SaveHotkeys(obs_data_t *obj);
	void SaveUISettings(obs_data_t *obj);
	void SaveVersion(obs_data_t *obj, const std::string &currentVersion);

	void LoadSettings(obs_data_t *obj);
	void LoadMacros(obs_data_t *obj);
	void LoadConnections(obs_data_t *obj);
	void LoadVariables(obs_data_t *obj);
	void LoadGeneralSettings(obs_data_t *obj);
	void LoadHotkeys(obs_data_t *obj);
	void LoadUISettings(obs_data_t *obj);

	bool VersionChanged(obs_data_t *obj, std::string currentVersion);
	bool TabOrderValid();
	void ResetTabOrder();
	bool PrioFuncsValid();

	/* --- End of saving / loading section --- */

	SwitcherData(obs_module_t *m, translateFunc t);
	inline ~SwitcherData() { Stop(); }

public:
	SwitcherThread *th = nullptr;
	std::mutex m;
	std::unique_lock<std::mutex> *mainLoopLock = nullptr;
	bool stop = false;
	std::condition_variable cv;
	std::condition_variable macroWaitCv;
	std::atomic_bool abortMacroWait = {false};
	std::condition_variable macroTransitionCv;

	std::vector<std::function<void()>> resetForNextIntervalFuncs;

	bool firstBoot = true;
	bool transitionActive = false;
	bool sceneColletionStop = false;
	bool replayBufferSaved = false;
	bool obsIsShuttingDown = false;
	bool firstInterval = true;
	bool firstIntervalAfterStop = true;
	int shutdownConditionCount = 0;
	bool startupLoadDone = false;

	obs_source_t *waitScene = nullptr;
	OBSWeakSource currentScene = nullptr;
	OBSWeakSource previousScene = nullptr;
	std::chrono::high_resolution_clock::time_point lastSceneChangeTime{};
	std::chrono::high_resolution_clock::time_point lastTransitionEndTime{};
	std::chrono::high_resolution_clock::time_point lastStreamStartingTime{};
	std::chrono::high_resolution_clock::time_point lastStreamStoppingTime{};
	std::chrono::high_resolution_clock::time_point lastMatchTime;

	/* --- Start of General tab section --- */

	int interval = default_interval;
	OBSWeakSource nonMatchingScene;
	enum class NoMatch { NO_SWITCH = 0, SWITCH = 1, RANDOM_SWITCH = 2 };
	NoMatch switchIfNotMatching = NoMatch::NO_SWITCH;
	Duration noMatchDelay;
	enum class StartupBehavior { PERSIST = 0, START = 1, STOP = 2 };
	StartupBehavior startupBehavior = StartupBehavior::PERSIST;
	enum class AutoStart {
		NEVER,
		RECORDING,
		STREAMING,
		RECORINDG_OR_STREAMING
	};
	AutoStart autoStartEvent = AutoStart::NEVER;
	Duration cooldown;
	bool showSystemTrayNotifications = false;
	bool transitionOverrideOverride = false;
	bool adjustActiveTransitionType = true;
	bool verbose = false;

	/* --- End of General tab section --- */

	struct AudioFadeInfo {
		std::atomic_bool active = {false};
		std::atomic_int id = {0};
	};
	AudioFadeInfo masterAudioFade;
	std::unordered_map<std::string, AudioFadeInfo> activeAudioFades;

	MacroProperties macroProperties;
	std::deque<std::shared_ptr<Macro>> macros;
	bool macroSceneSwitched = false;

	Curlhelper curl;
	std::deque<std::shared_ptr<Item>> connections;
	std::vector<std::string> websocketMessages;
	std::deque<std::shared_ptr<Item>> variables;

	std::string lastTitle;
	std::string currentTitle;
	std::string currentForegroundProcess;
	std::pair<int, int> lastCursorPos = {0, 0};
	bool cursorPosChanged = false;

	std::vector<int> functionNamesByPriority =
		GetDefaultFunctionPriorityList();
	const std::vector<ThreadPrio> threadPriorities = GetThreadPrioMapping();
	uint32_t threadPriority = QThread::NormalPriority;

	/* --- Start of hotkey section --- */

	bool hotkeysRegistered = false;
	obs_hotkey_id startHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id stopHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id toggleHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id upMacroSegment = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id downMacroSegment = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id removeMacroSegment = OBS_INVALID_HOTKEY_ID;

	/* --- End of hotkey section --- */

	/* --- Start of UI section --- */

	bool settingsWindowOpened = false;
	int lastOpenedTab = -1;
	std::string lastImportPath;
	QStringList loadFailureLibs;
	bool warnPluginLoadFailure = true;
	bool disableHints = false;
	bool disableFilterComboboxFilter = false;
	bool hideLegacyTabs = true;
	std::vector<int> tabOrder = std::vector<int>(tab_count);
	bool saveWindowGeo = false;
	QPoint windowPos = {};
	QSize windowSize = {};
	QList<int> macroActionConditionSplitterPosition;
	QList<int> macroListMacroEditSplitterPosition;

	/* --- End of UI section --- */

	/* --- Start of legacy tab section --- */

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
	void saveSceneGroups(obs_data_t *obj);
	void saveSceneTriggers(obs_data_t *obj);
	void saveVideoSwitches(obs_data_t *obj);
	void saveNetworkSwitches(obs_data_t *obj);

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
	void loadSceneGroups(obs_data_t *obj);
	void loadSceneTriggers(obs_data_t *obj);
	void loadVideoSwitches(obs_data_t *obj);
	void loadNetworkSettings(obs_data_t *obj);

	void Prune();

	bool checkSceneSequence(OBSWeakSource &scene, OBSWeakSource &transition,
				int &linger, bool &setPrevSceneAfterLinger);
	bool checkIdleSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	bool checkWindowTitleSwitch(OBSWeakSource &scene,
				    OBSWeakSource &transition);
	bool checkExeSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	bool checkScreenRegionSwitch(OBSWeakSource &scene,
				     OBSWeakSource &transition);
	bool checkSwitchInfoFromFile(OBSWeakSource &scene,
				     OBSWeakSource &transition);
	bool checkFileContent(OBSWeakSource &scene, OBSWeakSource &transition);
	bool checkRandom(OBSWeakSource &scene, OBSWeakSource &transition,
			 int &delay);
	bool checkMediaSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	bool checkTimeSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	bool checkAudioSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	void checkAudioSwitchFallback(OBSWeakSource &scene,
				      OBSWeakSource &transition);
	bool checkVideoSwitch(OBSWeakSource &scene, OBSWeakSource &transition);
	void checkTriggers();
	bool checkPause();
	void checkDefaultSceneTransitions();
	void writeSceneInfoToFile();
	void writeToStatusFile(const QString &msg);
	void checkSwitchCooldown(bool &match);

	std::deque<WindowSwitch> windowSwitches;
	std::vector<std::string> ignoreWindowsSwitches;
	IdleData idleData;
	std::vector<std::string> ignoreIdleWindows;
	bool showFrame = false;
	std::deque<ScreenRegionSwitch> screenRegionSwitches;
	bool uninterruptibleSceneSequenceActive = false;
	std::deque<SceneSequenceSwitch> sceneSequenceSwitches;
	std::deque<RandomSwitch> randomSwitches;
	OBSWeakSource lastRandomScene;
	FileIOData fileIO;
	std::deque<FileSwitch> fileSwitches;
	std::deque<ExecutableSwitch> executableSwitches;
	std::deque<SceneTrigger> sceneTriggers;
	std::deque<SceneTransition> sceneTransitions;
	std::deque<DefaultSceneTransition> defaultSceneTransitions;
	std::deque<MediaSwitch> mediaSwitches;
	std::deque<PauseEntry> pauseEntries;
	std::deque<TimeSwitch> timeSwitches;
	QDateTime liveTime;
	std::deque<AudioSwitch> audioSwitches;
	AudioSwitchFallback audioFallback;
	WSServer server;
	ServerStatus serverStatus = ServerStatus::NOT_RUNNING;
	WSClient client;
	ClientStatus clientStatus = ClientStatus::DISCONNECTED;
	NetworkConfig networkConfig;
	std::deque<VideoSwitch> videoSwitches;
	std::deque<SceneGroup> sceneGroups;
	SceneGroup *lastRandomSceneGroup = nullptr;

	/* --- End of legacy tab section --- */
private:
	obs_module_t *_modulePtr = nullptr;
	translateFunc _translate = nullptr;
};

class SwitcherThread : public QThread {
public:
	explicit SwitcherThread(){};
	void run() { switcher->Thread(); };
};

} // namespace advss
