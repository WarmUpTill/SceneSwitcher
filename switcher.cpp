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
#include <CoreGraphics/CGEvent.h>
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
	bool pauseSwitching = false;
	size_t sleepTime = 1000;
	auto locked = unique_lock<mutex>(mtx);
	while (isRunning) {
		//get Scene Name
		obs_source_t * transitionUsed = obs_get_output_source(0);
		obs_source_t * sceneUsed = obs_transition_get_active_source(transitionUsed);
		const char *sceneUsedName = obs_source_get_name(sceneUsed);
        obs_source_release(sceneUsed);
        obs_source_release(transitionUsed);
		//check if scene switching should be paused
		if ((sceneUsedName) && find(pauseScenes.begin(), pauseScenes.end(), string(sceneUsedName)) != pauseScenes.end()) {
			pauseSwitching = true;
		}
		else {
			pauseSwitching = false;
		}
		if (sceneRoundTripActive) {
			//did the user switch to another scene during scene round trip ?
			if ((sceneUsedName) && find(sceneRoundTrip.first.begin(), sceneRoundTrip.first.end(), string(sceneUsedName)) == sceneRoundTrip.first.end()) {
				sceneRoundTripActive = false;
				roundTripPos = 0;
			}
		}
		//check if a Scene Round Trip should be started
		else if ((sceneUsedName) && sceneRoundTrip.first.size() > 0 && strcmp(sceneUsedName, sceneRoundTrip.first.front().c_str()) == 0) {
			sceneRoundTripActive = true;
			roundTripPos = 0;
		}
		//are we in pause mode?
		if (!pauseSwitching) {
			///////////////////////////////////////////////
			//finding the scene to switch to
			///////////////////////////////////////////////
			if (!sceneRoundTripActive) {
				//get active window title
				windowname = GetActiveWindowTitle();
				//should we ignore this window name?
				if (find(ignoreNames.begin(), ignoreNames.end(), windowname) == ignoreNames.end()) {
					//reset values
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
								if (regex_match(windowname, regex(iter->first))) {
									sceneName = iter->second.sceneName;
									checkFullscreen = iter->second.isFullscreen;
									match = true;
									break;
								}
							}
							catch (...) {}
						}
					}
					///////////////////////////////////////////////
					//cursor regions check
					///////////////////////////////////////////////
					if (!match)
					{
						pair<int, int> cursorPos = getCursorXY();
						// compare against rectangular region format
						regex rules = regex("<-?[0-9]+x-?[0-9]+.-?[0-9]+x-?[0-9]+>");
						int minRegionSize = 99999;
						for (map<string, Data>::iterator iter = settingsMap.begin(); iter != settingsMap.end(); ++iter)
						{
							try
							{
								if (regex_match(iter->first, rules))
								{
									int minX, minY, maxX, maxY;
									int parsedValues = sscanf(iter->first.c_str(), "<%dx%d.%dx%d>", &minX, &minY, &maxX, &maxY);

									if (parsedValues == 4)
									{
										if (cursorPos.first >= minX && cursorPos.second >= minY && cursorPos.first <= maxX && cursorPos.second <= maxY)
										{
											// prioritize smaller regions over larger regions
											int regionSize = (maxX - minX) + (maxY - minY);
											if (regionSize < minRegionSize)
											{
												match = true;
												sceneName = iter->second.sceneName;
												checkFullscreen = iter->second.isFullscreen;
												minRegionSize = regionSize;
												// break;
											}
										}
									}
								}
							}
							catch (...) {}
						}
					}
				}
			}
			///////////////////////////////////////////////
			//switch the scene
			///////////////////////////////////////////////
			//do we only switch if window is also fullscreen? || are we in sceneRoundTripActive?
			match = (match && (!checkFullscreen || (checkFullscreen && isWindowFullscreen()))) || sceneRoundTripActive;
			//match or backup scene set
			if (settingsMap.find("Backup Scene Name") != settingsMap.end() || match) {
				//no match -> backup scene
				if (!match) {
					sceneName = settingsMap.find("Backup Scene Name")->second.sceneName;
				}
				if (sceneRoundTripActive) {
					sceneName = sceneRoundTrip.first[roundTripPos];
				}
				//check if current scene is already the desired scene
                transitionUsed = obs_get_output_source(0);
                sceneUsed = obs_transition_get_active_source(transitionUsed);
                const char *sceneUsedName = obs_source_get_name(sceneUsed);
                obs_source_release(sceneUsed);
				if ((sceneUsedName) && strcmp(sceneUsedName, sceneName.c_str()) != 0) {
					//switch scene
					obs_source_t *source = obs_get_source_by_name(sceneName.c_str()); //<--- do some more testing here regarding scene duplication
					if (source != NULL) {
						//create transition to new scene (otherwise UI wont work anymore)
						obs_transition_start(transitionUsed, OBS_TRANSITION_MODE_AUTO, 300, source); //OBS_TRANSITION_MODE_AUTO uses the obs user settings for transitions
					}
					obs_source_release(source);
				}
                obs_source_release(transitionUsed);
			}
			///////////////////////////////////////////////
			//Scene Round Trip
			///////////////////////////////////////////////
			if (sceneRoundTripActive) {
				//get delay and wait
				try {
					sleepTime = stoi(sceneRoundTrip.second[roundTripPos]) * 1000;
				}
				catch (...) {
					//just wait for 3 seconds if value was not set properly
					sleepTime = 3000;
				}
				//are we done with the Scene Round trip?
				if (roundTripPos + 1 == sceneRoundTrip.first.size()) {
					sceneRoundTripActive = false;
					roundTripPos = 0;
				}
				else {
					//prepare for next sceneName,Delay pair
					roundTripPos++;
				}
			}
		}
		//sleep for a bit
		terminate.wait_for(locked, chrono::milliseconds(sleepTime));
		//this_thread::sleep_for(chrono::milliseconds(sleepTime));
		sleepTime = 1000;
	}
}

#ifdef _WIN32
pair<int, int> Switcher::getCursorXY() {
	pair<int, int> pos(0, 0);
	POINT cursorPos;
	if (GetPhysicalCursorPos(&cursorPos)) {
		pos.first = cursorPos.x;
		pos.second = cursorPos.y;
	}
	return pos;
}
#endif

#ifdef __APPLE__ 
pair<int, int> Switcher::getCursorXY() {
	pair<int, int> pos(0, 0);
	CGEventRef event = CGEventCreate(NULL);
	CGPoint cursorPos = CGEventGetLocation(event);
	CFRelease(event);
	pos.first = cursorPos.x;
	pos.second = cursorPos.y;
	return pos;
}
#endif

#ifdef _WIN32
void Switcher::firstLoad() {
	settings.load();
	settingsMap = settings.getMap();
	sceneRoundTrip = settings.getSceneRoundTrip();
	pauseScenes = settings.getPauseScenes();
	ignoreNames = settings.getIgnoreNames();
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
	pauseScenes = settings.getPauseScenes();
	ignoreNames = settings.getIgnoreNames();
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
	settingsMap = settings.getMap();
	sceneRoundTrip = settings.getSceneRoundTrip();
	pauseScenes = settings.getPauseScenes();
	ignoreNames = settings.getIgnoreNames();
}

//start thread
void Switcher::start()
{
	auto locked = unique_lock<mutex>(mtx);    
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
	auto locked = unique_lock<mutex>(mtx);
	isRunning = false;
	if (switcherThread.joinable()) switcherThread.join();
}

string Switcher::getSettingsFilePath()
{
	return settings.getSettingsFilePath();;
}

void Switcher::setSettingsFilePath(string path)
{
	settings.setSettingsFilePath(path);
}

Switcher::~Switcher()
{
	stop();
}

bool Switcher::getIsRunning()
{
	return isRunning;
}
