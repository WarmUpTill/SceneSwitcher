#include "../headers/hotkey.hpp"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGEvent.h>
#import <Carbon/Carbon.h>
#include <Carbon/Carbon.h>
#include <util/platform.h>
#include <vector>
#include <QStringList>
#include <QRegularExpression>
#include <map>
#include <thread>

bool canSimulateKeyPresses = false;

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);

	@autoreleasepool {
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			std::string name([[app objectForKey:@"kCGWindowName"]
						 UTF8String],
					 [[app objectForKey:@"kCGWindowName"]
						 lengthOfBytesUsingEncoding:
							 NSUTF8StringEncoding]);
			std::string owner(
				[[app objectForKey:@"kCGWindowOwnerName"]
					UTF8String],
				[[app objectForKey:@"kCGWindowOwnerName"]
					lengthOfBytesUsingEncoding:
						NSUTF8StringEncoding]);

			if (!name.empty() &&
			    find(windows.begin(), windows.end(), name) ==
				    windows.end())
				windows.emplace_back(name);

			if (!owner.empty() &&
			    find(windows.begin(), windows.end(), owner) ==
				    windows.end())
				windows.emplace_back(owner);
		}
	}
}

void GetWindowList(QStringList &windows)
{
	windows.clear();
	std::vector<std::string> temp;
	GetWindowList(temp);
	for (auto &w : temp) {
		windows << QString::fromStdString(w);
	}
}

void GetCurrentWindowTitle(std::string &title)
{
	title.resize(0);

	@autoreleasepool {
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionOnScreenOnly,
				kCGNullWindowID);
		for (NSDictionary *app in apps) {
			int layer =
				[[app objectForKey:@"kCGWindowLayer"] intValue];
			// True if window is frontmost
			if (layer == 0) {
				std::string name(
					[[app objectForKey:@"kCGWindowName"]
						UTF8String],
					[[app objectForKey:@"kCGWindowName"]
						lengthOfBytesUsingEncoding:
							NSUTF8StringEncoding]);
				std::string owner(
					[[app objectForKey:@"kCGWindowOwnerName"]
						UTF8String],
					[[app objectForKey:@"kCGWindowOwnerName"]
						lengthOfBytesUsingEncoding:
							NSUTF8StringEncoding]);

				if (!name.empty())
					title = name;
				else if (!owner.empty())
					title = owner;

				break;
			}
		}
	}
}

std::pair<int, int> getCursorPos()
{
	std::pair<int, int> pos(0, 0);
	CGEventRef event = CGEventCreate(NULL);
	CGPoint cursorPos = CGEventGetLocation(event);
	CFRelease(event);
	pos.first = cursorPos.x;
	pos.second = cursorPos.y;
	return pos;
}

bool isWindowOriginOnScreen(NSDictionary *app, NSScreen *screen,
			    bool fullscreen = false)
{
	NSArray *screens = [NSScreen screens];
	NSRect mainScreenFrame = [screens[0] frame];
	NSRect screenFrame;
	if (fullscreen) {
		screenFrame = [screen frame];
	} else {
		screenFrame = [screen visibleFrame];
	}
	NSRect windowBounds;
	CGRectMakeWithDictionaryRepresentation(
		(CFDictionaryRef)[app objectForKey:@"kCGWindowBounds"],
		&windowBounds);

	return (windowBounds.origin.x == screenFrame.origin.x &&
		(mainScreenFrame.size.height - screenFrame.size.height -
			 windowBounds.origin.y ==
		 screenFrame.origin.y));
}

bool isWindowMaximizedOnScreen(NSDictionary *app, NSScreen *screen)
{
	double maximizedTolerance = 0.99;
	NSRect screenFrame = [screen frame];
	NSRect windowBounds;
	CGRectMakeWithDictionaryRepresentation(
		(CFDictionaryRef)[app objectForKey:@"kCGWindowBounds"],
		&windowBounds);

	int sumX = windowBounds.origin.x + windowBounds.size.width;
	int sumY = windowBounds.origin.y + windowBounds.size.height;

	// Return false if window spans over multiple screens
	if (sumX > screenFrame.size.width) {
		return false;
	}
	if (sumY > screenFrame.size.height) {
		return false;
	}

	return ((double)sumX / (double)screenFrame.size.width) >
		       maximizedTolerance &&
	       ((double)sumY / (double)screenFrame.size.height) >
		       maximizedTolerance;
}

bool nameMachesPattern(std::string windowName, std::string pattern)
{
	return QString::fromStdString(windowName)
		.contains(QRegularExpression(QString::fromStdString(pattern)));
}

bool isMaximized(const std::string &title)
{
	@autoreleasepool {
		NSArray *screens = [NSScreen screens];
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			std::string name([[app objectForKey:@"kCGWindowName"]
						 UTF8String],
					 [[app objectForKey:@"kCGWindowName"]
						 lengthOfBytesUsingEncoding:
							 NSUTF8StringEncoding]);
			std::string owner(
				[[app objectForKey:@"kCGWindowOwnerName"]
					UTF8String],
				[[app objectForKey:@"kCGWindowOwnerName"]
					lengthOfBytesUsingEncoding:
						NSUTF8StringEncoding]);

			bool equals = (title == name || title == owner);
			bool matches = nameMachesPattern(name, title) ||
				       nameMachesPattern(owner, title);
			if (equals || matches) {
				for (NSScreen *screen in screens) {
					if (isWindowOriginOnScreen(app,
								   screen) &&
					    isWindowMaximizedOnScreen(app,
								      screen)) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool isWindowFullscreenOnScreen(NSDictionary *app, NSScreen *screen)
{
	NSRect screenFrame = [screen frame];
	NSRect windowBounds;
	CGRectMakeWithDictionaryRepresentation(
		(CFDictionaryRef)[app objectForKey:@"kCGWindowBounds"],
		&windowBounds);

	return NSEqualSizes(windowBounds.size, screenFrame.size);
}

bool isFullscreen(const std::string &title)
{
	@autoreleasepool {
		NSArray *screens = [NSScreen screens];
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			std::string name([[app objectForKey:@"kCGWindowName"]
						 UTF8String],
					 [[app objectForKey:@"kCGWindowName"]
						 lengthOfBytesUsingEncoding:
							 NSUTF8StringEncoding]);
			std::string owner(
				[[app objectForKey:@"kCGWindowOwnerName"]
					UTF8String],
				[[app objectForKey:@"kCGWindowOwnerName"]
					lengthOfBytesUsingEncoding:
						NSUTF8StringEncoding]);

			bool equals = (title == name || title == owner);
			bool matches = nameMachesPattern(name, title) ||
				       nameMachesPattern(owner, title);
			if (equals || matches) {
				for (NSScreen *screen in screens) {
					if (isWindowOriginOnScreen(app, screen,
								   true) &&
					    isWindowFullscreenOnScreen(app,
								       screen))
						return true;
				}
			}
		}
	}
	return false;
}

int secondsSinceLastInput()
{
	double time = CGEventSourceSecondsSinceLastEventType(
			      kCGEventSourceStateCombinedSessionState,
			      kCGAnyInputEventType) +
		      0.5;
	return (int)time;
}

void GetProcessList(QStringList &list)
{
	list.clear();
	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		NSArray *array = [ws runningApplications];
		for (NSRunningApplication *app in array) {
			NSString *name = app.localizedName;
			if (!name)
				continue;

			const char *str = name.UTF8String;
			if (str && *str)
				list << (str);
		}
	}
}

bool isInFocus(const QString &executable)
{
	std::string current;
	GetCurrentWindowTitle(current);

	// True if executable switch equals current window
	bool equals = (executable.toStdString() == current);
	// True if executable switch matches current window
	bool matches = QString::fromStdString(current).contains(
		QRegularExpression(executable));

	return (equals || matches);
}

static std::map<HotkeyType, CGKeyCode> keyTable = {
	// Chars
	{HotkeyType::Key_A, kVK_ANSI_A},
	{HotkeyType::Key_B, kVK_ANSI_B},
	{HotkeyType::Key_C, kVK_ANSI_C},
	{HotkeyType::Key_D, kVK_ANSI_D},
	{HotkeyType::Key_E, kVK_ANSI_E},
	{HotkeyType::Key_F, kVK_ANSI_F},
	{HotkeyType::Key_G, kVK_ANSI_G},
	{HotkeyType::Key_H, kVK_ANSI_H},
	{HotkeyType::Key_I, kVK_ANSI_I},
	{HotkeyType::Key_J, kVK_ANSI_J},
	{HotkeyType::Key_K, kVK_ANSI_K},
	{HotkeyType::Key_L, kVK_ANSI_L},
	{HotkeyType::Key_M, kVK_ANSI_M},
	{HotkeyType::Key_N, kVK_ANSI_N},
	{HotkeyType::Key_O, kVK_ANSI_O},
	{HotkeyType::Key_P, kVK_ANSI_P},
	{HotkeyType::Key_Q, kVK_ANSI_Q},
	{HotkeyType::Key_R, kVK_ANSI_R},
	{HotkeyType::Key_S, kVK_ANSI_S},
	{HotkeyType::Key_T, kVK_ANSI_T},
	{HotkeyType::Key_U, kVK_ANSI_U},
	{HotkeyType::Key_V, kVK_ANSI_V},
	{HotkeyType::Key_W, kVK_ANSI_W},
	{HotkeyType::Key_X, kVK_ANSI_X},
	{HotkeyType::Key_Y, kVK_ANSI_Y},
	{HotkeyType::Key_Z, kVK_ANSI_Z},

	// Numbers
	{HotkeyType::Key_0, kVK_ANSI_0},
	{HotkeyType::Key_1, kVK_ANSI_1},
	{HotkeyType::Key_2, kVK_ANSI_2},
	{HotkeyType::Key_3, kVK_ANSI_3},
	{HotkeyType::Key_4, kVK_ANSI_4},
	{HotkeyType::Key_5, kVK_ANSI_5},
	{HotkeyType::Key_6, kVK_ANSI_6},
	{HotkeyType::Key_7, kVK_ANSI_7},
	{HotkeyType::Key_8, kVK_ANSI_8},
	{HotkeyType::Key_9, kVK_ANSI_9},

	{HotkeyType::Key_F1, kVK_F1},
	{HotkeyType::Key_F2, kVK_F2},
	{HotkeyType::Key_F3, kVK_F3},
	{HotkeyType::Key_F4, kVK_F4},
	{HotkeyType::Key_F5, kVK_F5},
	{HotkeyType::Key_F6, kVK_F6},
	{HotkeyType::Key_F7, kVK_F7},
	{HotkeyType::Key_F8, kVK_F8},
	{HotkeyType::Key_F9, kVK_F9},
	{HotkeyType::Key_F10, kVK_F10},
	{HotkeyType::Key_F11, kVK_F11},
	{HotkeyType::Key_F12, kVK_F12},
	{HotkeyType::Key_F13, kVK_F13},
	{HotkeyType::Key_F14, kVK_F14},
	{HotkeyType::Key_F15, kVK_F15},
	{HotkeyType::Key_F16, kVK_F16},
	{HotkeyType::Key_F17, kVK_F17},
	{HotkeyType::Key_F18, kVK_F18},
	{HotkeyType::Key_F19, kVK_F19},
	{HotkeyType::Key_F20, kVK_F20},
	// F21-F24 does not exist on MacOS

	{HotkeyType::Key_Escape, kVK_Escape},
	{HotkeyType::Key_Space, kVK_Space},
	{HotkeyType::Key_Return, kVK_Return},
	{HotkeyType::Key_Backspace, kVK_Delete},
	{HotkeyType::Key_Tab, kVK_Tab},

	{HotkeyType::Key_Shift_L, kVK_Shift},
	{HotkeyType::Key_Shift_R, kVK_RightShift},
	{HotkeyType::Key_Control_L, kVK_Control},
	{HotkeyType::Key_Control_R, kVK_RightControl},
	{HotkeyType::Key_Alt_L, kVK_Option},
	{HotkeyType::Key_Alt_R, kVK_RightOption},
	{HotkeyType::Key_Win_L, kVK_Command},
	{HotkeyType::Key_Win_R, kVK_RightCommand},
	// Not sure what Key_Apps would correspond to

	{HotkeyType::Key_CapsLock, kVK_CapsLock},
	{HotkeyType::Key_NumLock, kVK_ANSI_KeypadClear},
	{HotkeyType::Key_ScrollLock, kVK_F14},

	{HotkeyType::Key_PrintScreen, kVK_F13},
	{HotkeyType::Key_Pause, kVK_F15},

	{HotkeyType::Key_Insert, kVK_Help},
	{HotkeyType::Key_Delete, kVK_ForwardDelete},
	{HotkeyType::Key_PageUP, kVK_PageUp},
	{HotkeyType::Key_PageDown, kVK_PageDown},
	{HotkeyType::Key_Home, kVK_Home},
	{HotkeyType::Key_End, kVK_End},

	{HotkeyType::Key_Left, kVK_LeftArrow},
	{HotkeyType::Key_Up, kVK_UpArrow},
	{HotkeyType::Key_Right, kVK_RightArrow},
	{HotkeyType::Key_Down, kVK_DownArrow},

	{HotkeyType::Key_Numpad0, kVK_ANSI_Keypad0},
	{HotkeyType::Key_Numpad1, kVK_ANSI_Keypad1},
	{HotkeyType::Key_Numpad2, kVK_ANSI_Keypad2},
	{HotkeyType::Key_Numpad3, kVK_ANSI_Keypad3},
	{HotkeyType::Key_Numpad4, kVK_ANSI_Keypad4},
	{HotkeyType::Key_Numpad5, kVK_ANSI_Keypad5},
	{HotkeyType::Key_Numpad6, kVK_ANSI_Keypad6},
	{HotkeyType::Key_Numpad7, kVK_ANSI_Keypad7},
	{HotkeyType::Key_Numpad8, kVK_ANSI_Keypad8},
	{HotkeyType::Key_Numpad9, kVK_ANSI_Keypad9},

	{HotkeyType::Key_NumpadAdd, kVK_ANSI_KeypadPlus},
	{HotkeyType::Key_NumpadSubtract, kVK_ANSI_KeypadMinus},
	{HotkeyType::Key_NumpadMultiply, kVK_ANSI_KeypadMultiply},
	{HotkeyType::Key_NumpadDivide, kVK_ANSI_KeypadDivide},
	{HotkeyType::Key_NumpadDecimal, kVK_ANSI_KeypadDecimal},
	{HotkeyType::Key_NumpadEnter, kVK_ANSI_KeypadEnter},
};

void PressKeys(const std::vector<HotkeyType> keys, int duration)
{
	// TODO:
	// I can't seem to get this to work so drop support for this functionality
	// on MacOS
	canSimulateKeyPresses = false;
	return;

	// Check premissions
	NSDictionary *options =
		@{(__bridge id)kAXTrustedCheckOptionPrompt: @NO};
	if (!AXIsProcessTrustedWithOptions((CFDictionaryRef)options)) {
		NSString *urlString =
			@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";
		[[NSWorkspace sharedWorkspace]
			openURL:[NSURL URLWithString:urlString]];
		canSimulateKeyPresses = false;
		return;
	}

	long modifierFlags = 0;
	auto source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
	if (!source) {
		canSimulateKeyPresses = false;
		return;
	}

	// Press keys
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}

		CGKeyCode inputKeyCode = it->second;
		CGEventRef keyPress =
			CGEventCreateKeyboardEvent(source, inputKeyCode, true);
		if (!keyPress) {
			canSimulateKeyPresses = false;
			CFRelease(source);
			return;
		}
		CGEventSetFlags(keyPress, modifierFlags);
		CGEventPost(kCGAnnotatedSessionEventTap, keyPress);
		CFRelease(keyPress);

		switch (it->first) {
		case HotkeyType::Key_Shift_L:
		case HotkeyType::Key_Shift_R:
			modifierFlags |= NX_SHIFTMASK;
			break;
		case HotkeyType::Key_Control_L:
		case HotkeyType::Key_Control_R:
			modifierFlags |= NX_CONTROLMASK;
			break;
		case HotkeyType::Key_Alt_L:
		case HotkeyType::Key_Alt_R:
			modifierFlags |= NX_ALTERNATEMASK;
			break;
		case HotkeyType::Key_Win_L:
		case HotkeyType::Key_Win_R:
			modifierFlags |= NX_COMMANDMASK;
			break;
		default:
			break;
		}
	}

	// When instantly releasing the key presses OBS might miss them
	std::this_thread::sleep_for(std::chrono::milliseconds(300));

	// Release keys
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		CGKeyCode inputKeyCode = it->second;
		CGEventRef keyRelease =
			CGEventCreateKeyboardEvent(source, inputKeyCode, false);
		if (!keyRelease) {
			canSimulateKeyPresses = false;
			CFRelease(source);
			return;
		}
		CGEventPost(kCGAnnotatedSessionEventTap, keyRelease);
		CFRelease(keyRelease);
	}

	CFRelease(source);
}

void PlatformCleanup() {}

void PlatformInit() {}
