#include "platform-funcs.hpp"

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

namespace advss {

static std::string nsStringToStdString(NSString *str)
{
	if (!str) {
		return "";
	}
	const char *utf8 = [str UTF8String];
	return utf8 ? utf8 : "";
}

std::string GetCurrentWindowTitle()
{
	std::string title;
	@autoreleasepool {
		CFArrayRef cfApps = CGWindowListCopyWindowInfo(
			kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
		NSMutableArray *apps = (__bridge NSMutableArray *)cfApps;
		for (NSDictionary *app in apps) {
			int layer =
				[[app objectForKey:@"kCGWindowLayer"] intValue];

			if (layer != 0) {
				continue;
			}

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

			if (!name.empty()) {
				title = name;
			} else if (!owner.empty()) {
				title = owner;
			}

			// Ignore the "StatusIndicator" application window
			//
			// It is used to display the microphone status and if active is
			// always the top most window
			if (title == "StatusIndicator") {
				continue;
			}

			break;
		}
		apps = nil;
		CFRelease(cfApps);
	}
	return title;
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

bool isWindowFullscreenOnScreen(NSDictionary *app, NSScreen *screen)
{
	NSRect screenFrame = [screen frame];
	NSRect windowBounds;
	CGRectMakeWithDictionaryRepresentation(
		(CFDictionaryRef)[app objectForKey:@"kCGWindowBounds"],
		&windowBounds);

	return NSEqualSizes(windowBounds.size, screenFrame.size);
}

std::vector<WindowInfo> GetWindows(const WindowQueryOptions &options)
{
	std::vector<WindowInfo> result;
	const std::string foregroundTitle = GetCurrentWindowTitle();

	@autoreleasepool {
		NSArray *screens = [NSScreen screens];

		CFArrayRef cfApps = CGWindowListCopyWindowInfo(
			kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
		NSMutableArray *apps = (__bridge NSMutableArray *)cfApps;

		// Track titles already added to avoid duplicates (name + owner)
		std::vector<std::string> seen;

		for (NSDictionary *app in apps) {
			std::string name = nsStringToStdString(
				[app objectForKey:@"kCGWindowName"]);
			std::string owner = nsStringToStdString(
				[app objectForKey:@"kCGWindowOwnerName"]);

			// Collect geometry once (used for multiple fields)
			NSRect bounds = NSZeroRect;
			if (options.geometry || options.fullscreen ||
			    options.maximized) {
				CGRectMakeWithDictionaryRepresentation(
					(CFDictionaryRef)[app
						objectForKey:@"kCGWindowBounds"],
					&bounds);
			}

			// Process both the window name and the owner name as
			// separate entries, matching the old GetWindowList() behaviour
			for (int pass = 0; pass < 2; ++pass) {
				const std::string &title = (pass == 0) ? name
								       : owner;
				if (title.empty()) {
					continue;
				}
				if (std::find(seen.begin(), seen.end(),
					      title) != seen.end()) {
					continue;
				}
				seen.emplace_back(title);

				WindowInfo info;
				info.title = title;

				if (options.focus) {
					info.focused =
						(title == foregroundTitle);
				}

				if (options.geometry || options.fullscreen ||
				    options.maximized) {
					info.x = (int)bounds.origin.x;
					info.y = (int)bounds.origin.y;
					info.width = (int)bounds.size.width;
					info.height = (int)bounds.size.height;
				}

				if (options.fullscreen) {
					for (NSScreen *screen in screens) {
						if (isWindowOriginOnScreen(
							    app, screen,
							    true) &&
						    isWindowFullscreenOnScreen(
							    app, screen)) {
							info.fullscreen = true;
							break;
						}
					}
				}

				if (options.maximized) {
					for (NSScreen *screen in screens) {
						if (isWindowOriginOnScreen(
							    app, screen) &&
						    isWindowMaximizedOnScreen(
							    app, screen)) {
							info.maximized = true;
							break;
						}
					}
				}

				// windowClass and text not implemented on macOS
				result.emplace_back(std::move(info));
			}
		}
		apps = nil;
		CFRelease(cfApps);
	}
	return result;
}

int SecondsSinceLastInput()
{
	double time = CGEventSourceSecondsSinceLastEventType(
			      kCGEventSourceStateCombinedSessionState,
			      kCGAnyInputEventType) +
		      0.5;
	return (int)time;
}

QStringList GetProcessList()
{
	QStringList list;
	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		NSArray *array = [ws runningApplications];
		for (NSRunningApplication *app in array) {
			NSString *name = app.localizedName;
			if (!name) {
				continue;
			}

			const char *str = name.UTF8String;
			if (str && *str) {
				list << (str);
			}
		}
	}
	return list;
}

std::string GetForegroundProcessName()
{
	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		NSArray *array = [ws runningApplications];
		for (NSRunningApplication *app in array) {
			if (!app.isActive) {
				continue;
			}
			NSString *name = app.localizedName;
			if (!name) {
				break;
			}
			const char *str = name.UTF8String;
			if (str) {
				return str;
			}
			break;
		}
	}
	return {};
}

std::string GetForegroundProcessPath()
{
	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		for (NSRunningApplication *app in [ws runningApplications]) {
			if (!app.isActive) {
				continue;
			}
			NSURL *url = app.executableURL;
			if (!url) {
				break;
			}
			const char *str = url.path.UTF8String;
			if (str) {
				return str;
			}
			break;
		}
	}
	return {};
}

QStringList GetProcessPathsFromName(const QString &name)
{
	QStringList paths;
	@autoreleasepool {
		NSWorkspace *ws = [NSWorkspace sharedWorkspace];
		for (NSRunningApplication *app in [ws runningApplications]) {
			NSString *appName = app.localizedName;
			if (!appName) {
				continue;
			}
			const char *nameStr = appName.UTF8String;
			if (!nameStr || name != QString::fromUtf8(nameStr)) {
				continue;
			}
			NSURL *url = app.executableURL;
			if (!url) {
				continue;
			}
			const char *pathStr = url.path.UTF8String;
			if (!pathStr) {
				continue;
			}
			QString path = QString::fromUtf8(pathStr);
			if (!paths.contains(path)) {
				paths.append(path);
			}
		}
	}

	return paths;
}

bool IsInFocus(const QString &executable)
{
	const auto current = GetForegroundProcessName();

	// True if executable switch equals current window
	bool equals = (executable.toStdString() == current);
	// True if executable switch matches current window
	bool matches = QString::fromStdString(current).contains(
		QRegularExpression(executable));

	return (equals || matches);
}

void PlatformCleanup() {}

void PlatformInit() {}

} // namespace advss
