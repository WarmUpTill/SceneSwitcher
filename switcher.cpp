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
	while (isRunning) {
		//get active window title
		string windowname = GetActiveWindowTitle();
		bool match = false;
		string name = "";
		for (std::map<string, string>::iterator iter = settingsMap.begin(); iter != settingsMap.end(); ++iter)
		{
			try
			{
				regex e = regex(iter->first);
				match = regex_match(windowname, e);
				if (match) {
					name = iter->second;
					break;
				}
			}
			catch(exception const & ex)
			{

			}
		}
		//do we know the window title or is a fullscreen/backup Scene set?
		if (!(settingsMap.find("Fullscreen Scene Name") == settingsMap.end()) || !(settingsMap.find("Backup Scene Name") == settingsMap.end())||match){

			if (!match && !(settingsMap.find("Fullscreen Scene Name") == settingsMap.end()) && isWindowFullscreen()) {
				name = settingsMap.find("Fullscreen Scene Name")->second;
			}
			else if (!match && !(settingsMap.find("Backup Scene Name") == settingsMap.end())) {
				name = settingsMap.find("Backup Scene Name")->second;
			}
				obs_source_t * transitionUsed = obs_get_output_source(0);
				obs_source_t * sceneUsed = obs_transition_get_active_source(transitionUsed);
				const char *sceneUsedName = obs_source_get_name(sceneUsed);
				//check if current scene is already the desired scene
				if ((sceneUsedName) && strcmp(sceneUsedName, name.c_str()) != 0) {
					//switch scene
					obs_source_t *source = obs_get_source_by_name(name.c_str());
					if (source == NULL)
					{
						//warn("Source not found: \"%s\"", name);
						;
					}
					else if (obs_scene_from_source(source) == NULL)
					{
						//warn("\"%s\" is not a scene", name);
						;
					}
					else {
						//create transition to new scene (otherwise UI wont work anymore)
						//OBS_TRANSITION_MODE_AUTO uses the obs user settings for transitions
						obs_transition_start(transitionUsed, OBS_TRANSITION_MODE_AUTO, 300, source);
					}
					obs_source_release(source);
				}
				obs_source_release(sceneUsed);
				obs_source_release(transitionUsed);
		}
		//sleep for a bit
		this_thread::sleep_for(std::chrono::milliseconds(1000));
	}
}

#ifdef _WIN32
void Switcher::firstLoad() {
	settings.load();
	settingsMap = settings.getMap();
	if (settingsMap.find("Disable Start Message") == settingsMap.end() || settingsMap.find("Disable Start Message")->second != "Yes") {
		string message = "The following settings were found for Scene Switcher:\n";
		for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
		{
			message += (it->first) + " -> " + it->second + "\n";
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
    if (settingsMap.find("Disable Start Message") == settingsMap.end() || settingsMap.find("Disable Start Message")->second != "Yes") {
        string message = "The following settings were found for Scene Switcher:\n";
        for (auto it = settingsMap.cbegin(); it != settingsMap.cend(); ++it)
        {
            message += (it->first) + " -> " + it->second + "\n";
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
                                                  sizeof(keys)/sizeof(*keys),
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
    //TODO: implement the MAC OS version
    string cmd = "osascript -e
	'global frontApp, frontAppName, windowTitle
	set windowTitle to ""
	tell application \"System Events\"
		set frontApp to first application process whose frontmost is true
		set frontAppName to name of frontApp
		tell process frontAppName
			tell (1st window whose value of attribute \"AXMain\" is true)
				set windowTitle to value of attribute \"AXTitle\"
			end tell
		end tell
	end tell

	tell application frontAppName
		get bounds of front window
	end tell'";
    return false;
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
    string cmd = "osascript -e 'tell application \"System Events\"' -e 'set frontApp to name of first application process whose frontmost is true' -e 'end tell'";
    char buffer[256];
    FILE * f = popen(cmd.c_str(), "r");
    fgets(buffer, 255, f);
    pclose(f);
    return buffer;
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