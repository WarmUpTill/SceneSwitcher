#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CGEvent.h>
#import <Carbon/Carbon.h>
#include <Carbon/Carbon.h>
#include <util/platform.h>
#include <vector>
#include <QStringList>
#include <QRegularExpression>

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);

	@autoreleasepool {
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			// Construct string from NSString accounting for nil
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

			// Check if name exists
			if (!name.empty() &&
			    find(windows.begin(), windows.end(), name) ==
				    windows.end())
				windows.emplace_back(name);
			// Check if owner exists
			else if (!owner.empty() &&
				 find(windows.begin(), windows.end(), owner) ==
					 windows.end())
				windows.emplace_back(owner);
		}
	}
}

// Overloaded
void GetWindowList(QStringList &windows)
{
	windows.clear();

	@autoreleasepool {
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			// Construct string from NSString accounting for nil
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

			// Check if name exists
			if (!name.empty() &&
			    !windows.contains(QString::fromStdString(name)))
				windows << QString::fromStdString(name);
			// Check if owner exists
			else if (!owner.empty() &&
				 !windows.contains(
					 QString::fromStdString(name)))
				windows << QString::fromStdString(owner);
		}
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
				// Construct string from NSString accounting for nil
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

// TODO:
// not implemented on MacOS as I cannot test it
bool isMaximized(std::string &title)
{
	return false;
}

bool isFullscreen(std::string &title)
{
	// Check for match
	@autoreleasepool {
		NSArray *screens = [NSScreen screens];
		NSMutableArray *apps =
			(__bridge NSMutableArray *)CGWindowListCopyWindowInfo(
				kCGWindowListOptionAll, kCGNullWindowID);
		for (NSDictionary *app in apps) {
			// Construct string from NSString accounting for nil
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

			// True if switch equals app
			bool equals = (title == name || title == owner);
			// True if switch matches app
			bool matches = (QString::fromStdString(name).contains(
						QRegularExpression(
							QString::fromStdString(
								title))) ||
					QString::fromStdString(owner).contains(
						QRegularExpression(
							QString::fromStdString(
								title))));

			// If found, check if fullscreen
			if (equals || matches) {
				// Get window bounds
				NSRect bounds;
				CGRectMakeWithDictionaryRepresentation(
					(CFDictionaryRef)[app
						objectForKey:@"kCGWindowBounds"],
					&bounds);

				// Compare to screen bounds
				for (NSScreen *screen in screens) {
					NSRect frame = [screen visibleFrame];

					// True if flipped window origin equals screen origin
					bool origin =
						(bounds.origin.x ==
							 frame.origin.x &&
						 ([screens[0] visibleFrame]
								  .size.height -
							  frame.size.height -
							  bounds.origin.y ==
						  frame.origin.y));
					// True if window size equals screen size
					bool size = NSEqualSizes(bounds.size,
								 frame.size);

					if (origin && size)
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
