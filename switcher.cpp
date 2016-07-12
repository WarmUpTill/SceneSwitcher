#include "settings.h"
#include <map>
#include <string>
#include <chrono>
#include <obs.h>
#include <thread>
#include <regex>
#include "switcher.h"
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif
using namespace std;

//scene switching is done in here
void Switcher::switcherThreadFunc() {
	bool sceneRoundTripActive = false;
	size_t roundTripPos = 0;
	bool match = false;
	string windowname = "";
	string sceneName = "";
	bool checkFullscreen = false;
	while (isRunning) {
		//get Scene Name
		obs_source_t * transitionUsed = obs_get_output_source(0);
		obs_source_t * sceneUsed = obs_transition_get_active_source(transitionUsed);
		const char *sceneUsedName = obs_source_get_name(sceneUsed);
		//check if a Scene Round Trip should be started
		if ((sceneUsedName) && strcmp(sceneUsedName, sceneRoundTrip.front().c_str()) == 0) {
			sceneRoundTripActive = true;
			roundTripPos = 1;
		}
		if (sceneRoundTripActive) {
			//get delay and wait
			try {
				this_thread::sleep_for(chrono::milliseconds(1000 * stoi(sceneRoundTrip.at(roundTripPos), nullptr, 10)));
			}
			catch (...) {
				//just wait for 3 seconds if value was not set properly
				this_thread::sleep_for(chrono::milliseconds(3000));
			}
			//are we done with the Scene Round trip?
			if (roundTripPos + 1 >= sceneRoundTrip.size()) {
				sceneRoundTripActive = false;
				roundTripPos = 0;
			}
			else {
				//switch scene
				sceneName = sceneRoundTrip.at(roundTripPos + 1);
				obs_source_t * transitionUsed = obs_get_output_source(0);
				obs_source_t *source = obs_get_source_by_name(sceneName.c_str());
				if (source != NULL) {
					//create transition to new scene
					obs_transition_start(transitionUsed, OBS_TRANSITION_MODE_AUTO, 300, source); //OBS_TRANSITION_MODE_AUTO uses the obs user settings for transitions
				}
				obs_source_release(source);
				//prepare for next sceneName,Delay pair
				roundTripPos += 2;
			}
		}
		//normal scene switching here
		else {
			//get active window title and reset values
			windowname = GetActiveWindowTitle();
			match = false;
			sceneName = "";
			checkFullscreen = false;
			//first check if there is a direct match
			map<string, Data>::iterator it = settingsMap.find(windowname);
			if (it != settingsMap.end()) {
				sceneName = it->second.sceneName;
				checkFullscreen = it->second.isFullscreen;
				match = true;
			}
			else {
				//maybe there is a regular expression match
				for (map<string, Data>::iterator iter = settingsMap.begin(); iter != settingsMap.end(); ++iter)
				{
					try {
						regex e = regex(iter->first);
						match = regex_match(windowname, e);
						if (match) {
							sceneName = iter->second.sceneName;
							checkFullscreen = iter->second.isFullscreen;
							break;
						}
					}
					catch (...) {}
				}
			}
			//do we only switch if window is also fullscreen?
			if (!checkFullscreen || (checkFullscreen && isWindowFullscreen())) {
				//do we know the window title or is a fullscreen/backup Scene set?
				if (!(settingsMap.find("Backup Scene Name") == settingsMap.end()) || match) {
					if (!match && !(settingsMap.find("Backup Scene Name") == settingsMap.end())) {
						sceneName = settingsMap.find("Backup Scene Name")->second.sceneName;
					}
					//check if current scene is already the desired scene
					if ((sceneUsedName) && strcmp(sceneUsedName, sceneName.c_str()) != 0) {
						//switch scene
						obs_source_t *source = obs_get_source_by_name(sceneName.c_str());
						if (source != NULL) {
							//create transition to new scene (otherwise UI wont work anymore)
							obs_transition_start(transitionUsed, OBS_TRANSITION_MODE_AUTO, 300, source); //OBS_TRANSITION_MODE_AUTO uses the obs user settings for transitions
						}
						obs_source_release(source);
					}
					obs_source_release(sceneUsed);
					obs_source_release(transitionUsed);
				}
			}
			//sleep for a bit
			this_thread::sleep_for(chrono::milliseconds(1000));
		}
	}
}

#ifdef _WIN32
void Switcher::firstLoad() {
	settings.load();
	settingsMap = settings.getMap();
	sceneRoundTrip = settings.getSceneRoundTrip();
	if (!settings.getStartMessageDisable()) {
		string message = "The following settings were found for Scene Switcher:\n";
		for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
		{
			message += (it->first) + " -> " + it->second.sceneName + "\n";
		}
		message += "\n(settings file located at: " + settings.getSettingsFilePath() + ")";
		MessageBoxA(0, message.c_str(), "Scene Switcher", 0);
	}
}
#endif

#ifdef 	__APPLE__ 
void Switcher::firstLoad() {
	settings.load();
	settingsMap = settings.getMap();
	sceneRoundTrip = settings.getSceneRoundTrip();
	if (!settings.getStartMessageDisable()) {
		string message = "The following settings were found for Scene Switcher:\n";
		for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
		{
			message += (it->first) + " -> " + it->second.sceneName + "\n";
		}
		message += "\n(settings file located at: " + settings.getSettingsFilePath() + ")";
		SInt32 nRes = 0;
		CFUserNotificationRef pDlg = NULL;
		const void* keys[] = { kCFUserNotificationAlertHeaderKey,
			kCFUserNotificationAlertMessageKey };
		const void* vals[] = {
			CFSTR("Test Foundation Message Box"),
			CFStringCreateWithCString(kCFAllocatorDefault,message.c_str(),kCFStringEncodingMacRoman)
		};

		CFDictionaryRef dict = CFDictionaryCreate(0, keys, vals,
			sizeof(keys) / sizeof(*keys),
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);

		pDlg = CFUserNotificationCreate(kCFAllocatorDefault, 0,
			kCFUserNotificationPlainAlertLevel,
			&nRes, dict);
	}
}
#endif


//load the settings needed to start the thread
void Switcher::load() {
	settings.load();
	sceneRoundTrip = settings.getSceneRoundTrip();
	settingsMap = settings.getMap();
}

//start thread
void Switcher::start()
{
	isRunning = true;
	switcherThread = thread(&Switcher::switcherThreadFunc, this);
}

//checks if active window is in fullscreen
#ifdef _WIN32
bool Switcher::isWindowFullscreen() {
	RECT appBounds;
	RECT rc;
	GetWindowRect(GetDesktopWindow(), &rc);
	HWND hwnd = GetForegroundWindow();
	//Check we haven't picked up the desktop or the shell
	if (hwnd != GetDesktopWindow() || hwnd != GetShellWindow())
	{
		GetWindowRect(hwnd, &appBounds);
		//determine if window is fullscreen
		if (rc.bottom == appBounds.bottom && rc.top == appBounds.top && rc.left == appBounds.left && rc.right == appBounds.right)
		{
			return true;
		}
	}
	return false;
}
#endif

#ifdef __APPLE__
bool Switcher::isWindowFullscreen() {
	//get screen resolution
	string cmd = "osascript -e 'tell application \"Finder\" to get the bounds of the window of the desktop'";
	char resolution[256];
	FILE * f1 = popen(cmd.c_str(), "r");
	fgets(resolution, 255, f1);
	pclose(f1);
	string resolutionString = string(resolution);

	//get window resolution
	cmd = "osascript "
		"-e 'global frontApp, frontAppName, windowTitle, boundsValue' "
		"-e 'set windowTitle to \"\"' "
		"-e 'tell application \"System Events\"' "
		"-e 'set frontApp to first application process whose frontmost is true' "
		"-e 'set frontAppName to name of frontApp' "
		"-e 'tell process frontAppName' "
		"-e 'tell (1st window whose value of attribute \"AXMain\" is true)' "
		"-e 'set windowTitle to value of attribute \"AXTitle\"' "
		"-e 'end tell' "
		"-e 'end tell' "
		"-e 'end tell' "
		"-e 'tell application frontAppName' "
		"-e 'set boundsValue to bounds of front window' "
		"-e 'end tell' "
		"-e 'return boundsValue' ";


	char bounds[256];
	FILE * f2 = popen(cmd.c_str(), "r");
	fgets(bounds, 255, f2);
	pclose(f2);
	string boundsString = string(bounds);

	return resolutionString.compare(boundsString) == 0;
}
#endif

//reads the title of the currently active window
#ifdef _WIN32
string Switcher::GetActiveWindowTitle()
{
	char wnd_title[256];
	//get handle of currently active window
	HWND hwnd = GetForegroundWindow();
	GetWindowTextA(hwnd, wnd_title, sizeof(wnd_title));
	return wnd_title;
}
#endif

#ifdef 	__APPLE__
string Switcher::GetActiveWindowTitle()
{
	string cmd = "osascript "
		"-e 'global frontApp, frontAppName, windowTitle' "
		"-e 'set windowTitle to \"\"' "
		"-e 'tell application \"System Events\"' "
		"-e 'set frontApp to first application process whose frontmost is true' "
		"-e 'set frontAppName to name of frontApp' "
		"-e 'tell process frontAppName' "
		"-e 'tell (1st window whose value of attribute \"AXMain\" is true)' "
		"-e 'set windowTitle to value of attribute \"AXTitle\"' "
		"-e 'end tell' "
		"-e 'end tell' "
		"-e 'end tell' "
		"-e 'return windowTitle' ";

	char buffer[256];
	FILE * f = popen(cmd.c_str(), "r");
	fgets(buffer, 255, f);
	pclose(f);
	//osascript adds carriage return that we need to remove
	string windowname = string(buffer);
	windowname.pop_back();
	return windowname;
}
#endif


//tell the thread to stop
void Switcher::stop() {
	isRunning = false;
	switcherThread.join();
	return;
}

string Switcher::getSettingsFilePath()
{
	return settings.getSettingsFilePath();;
}

void Switcher::setSettingsFilePath(string path)
{
	settings.setSettingsFilePath(path);
}

bool Switcher::getIsRunning()
{
	return isRunning;
}