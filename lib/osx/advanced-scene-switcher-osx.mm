#include "platform-funcs.hpp"

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGEvent.h>
#import <Carbon/Carbon.h>
#include <Carbon/Carbon.h>
#include <util/platform.h>
#include <set>
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


bool isWindowMaximizedOnScreen(NSDictionary *app, NSScreen *screen)
{
	NSRect windowBounds;
	CGRectMakeWithDictionaryRepresentation(
		(CFDictionaryRef)[app objectForKey:@"kCGWindowBounds"],
		&windowBounds);

	// CGWindowList: flipped coords, origin at top-left of main display.
	// NSScreen: Cocoa coords, origin at bottom-left of main display.
	NSRect mainFrame = [[NSScreen screens][0] frame];
	NSRect visFrame = [screen visibleFrame];

	// Convert visible area to CGWindow coordinates.
	// screenTop is just below the menubar; screenBottom is just above the
	// dock.
	CGFloat screenLeft = visFrame.origin.x;
	CGFloat screenTop =
		mainFrame.size.height - visFrame.origin.y - visFrame.size.height;
	CGFloat screenRight = screenLeft + visFrame.size.width;
	CGFloat screenBottom = screenTop + visFrame.size.height;

	const double tolerance = 4.0;

	// Width must span the full visible width.
	if (fabs(windowBounds.origin.x - screenLeft) > tolerance) {
		return false;
	}
	if (fabs((windowBounds.origin.x + windowBounds.size.width) -
		 screenRight) > tolerance) {
		return false;
	}
	// Bottom edge must align with the visible bottom (above dock).
	if (fabs((windowBounds.origin.y + windowBounds.size.height) -
		 screenBottom) > tolerance) {
		return false;
	}
	// Top edge must be at or above the menubar line (some apps extend
	// their window frame behind the menubar).
	if (windowBounds.origin.y > screenTop + tolerance) {
		return false;
	}
	return true;
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
			kCGWindowListExcludeDesktopElements, kCGNullWindowID);
		NSMutableArray *apps = (__bridge NSMutableArray *)cfApps;

		// Pre-compute which PIDs own at least one fullscreen-sized window.
		// A fullscreen app reports a small chrome/title bar window first and
		// its full-size content window second; checking only the first
		// window's bounds gives a false negative.
		std::set<int> fullscreenPIDs;
		if (options.fullscreen) {
			for (NSDictionary *app in apps) {
				int layer = [[app objectForKey:@"kCGWindowLayer"]
					intValue];
				if (layer != 0) {
					continue;
				}
				for (NSScreen *screen in screens) {
					if (isWindowFullscreenOnScreen(app,
								       screen)) {
						int pid = [[app objectForKey:
							@"kCGWindowOwnerPID"]
							intValue];
						fullscreenPIDs.insert(pid);
						break;
					}
				}
			}
		}

		std::set<int> maximizedPIDs;
		if (options.maximized) {
			for (NSDictionary *app in apps) {
				int layer = [[app objectForKey:@"kCGWindowLayer"]
					intValue];
				if (layer != 0) {
					continue;
				}
				for (NSScreen *screen in screens) {
					if (isWindowMaximizedOnScreen(app,
								      screen)) {
						int pid = [[app objectForKey:
							@"kCGWindowOwnerPID"]
							intValue];
						maximizedPIDs.insert(pid);
						break;
					}
				}
			}
		}

		// Pre-compute the largest-area window bounds per PID.
		// CGWindowList often lists small auxiliary windows (toolbars,
		// chrome) before the main content window; using the largest area
		// avoids reporting those wrong bounds for geometry.
		std::map<int, NSRect> largestBoundsPerPID;
		if (options.geometry || options.maximized) {
			for (NSDictionary *app in apps) {
				int layer = [[app objectForKey:@"kCGWindowLayer"]
					intValue];
				if (layer != 0) {
					continue;
				}
				NSRect b = NSZeroRect;
				CGRectMakeWithDictionaryRepresentation(
					(CFDictionaryRef)[app
						objectForKey:@"kCGWindowBounds"],
					&b);
				int pid = [[app objectForKey:@"kCGWindowOwnerPID"]
					intValue];
				auto it = largestBoundsPerPID.find(pid);
				if (it == largestBoundsPerPID.end() ||
				    b.size.width * b.size.height >
					    it->second.size.width *
						    it->second.size.height) {
					largestBoundsPerPID[pid] = b;
				}
			}
		}

		// Track titles already added to avoid duplicates (name + owner)
		std::vector<std::string> seen;

		for (NSDictionary *app in apps) {
			int layer =
				[[app objectForKey:@"kCGWindowLayer"] intValue];
			if (layer != 0) {
				continue;
			}

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
					int pid = [[app objectForKey:
						@"kCGWindowOwnerPID"]
						intValue];
					auto it = largestBoundsPerPID.find(pid);
					const NSRect &geoBounds =
						(it != largestBoundsPerPID.end())
							? it->second
							: bounds;
					info.x = (int)geoBounds.origin.x;
					info.y = (int)geoBounds.origin.y;
					info.width = (int)geoBounds.size.width;
					info.height = (int)geoBounds.size.height;

					if (options.fullscreen) {
						info.fullscreen =
							fullscreenPIDs.count(pid) >
							0;
					}

					if (options.maximized) {
						info.maximized =
							maximizedPIDs.count(pid) >
							0;
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
