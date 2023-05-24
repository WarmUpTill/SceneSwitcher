#include "platform-funcs.hpp"
#include "hotkey.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/scrnsaver.h>
#include <X11/extensions/XTest.h>
#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Status
#undef Unsorted
#include <util/platform.h>
#include <vector>
#include <thread>
#include <unordered_map>
#include <QStringList>
#include <QRegularExpression>
#include <QLibrary>
#include <fstream>
#include <sstream>

namespace advss {

static Display *xdisplay = 0;

static QLibrary *libXtstHandle = nullptr;
typedef int (*keyPressFunc)(Display *, unsigned int, bool, unsigned long);
static keyPressFunc pressFunc = nullptr;
bool canSimulateKeyPresses = false;

static QLibrary *libXssHandle = nullptr;
typedef XScreenSaverInfo *(*XScreenSaverAllocInfoFunc)();
typedef int (*XScreenSaverQueryInfoFunc)(Display *, Window, XScreenSaverInfo *);
static XScreenSaverAllocInfoFunc allocSSFunc = nullptr;
static XScreenSaverQueryInfoFunc querySSFunc = nullptr;
bool canGetIdleTime = false;

std::chrono::high_resolution_clock::time_point lastMouseLeftClickTime{};
std::chrono::high_resolution_clock::time_point lastMouseMiddleClickTime{};
std::chrono::high_resolution_clock::time_point lastMouseRightClickTime{};

Display *disp()
{
	if (!xdisplay) {
		xdisplay = XOpenDisplay(NULL);
	}

	return xdisplay;
}

void cleanupDisplay()
{
	if (!xdisplay) {
		return;
	}

	XCloseDisplay(xdisplay);
	xdisplay = 0;
}

static bool ewmhIsSupported()
{
	Display *display = disp();
	if (!display) {
		return false;
	}
	auto window = DefaultRootWindow(display);
	if (!window) {
		return false;
	}

	Atom netSupportingWmCheck =
		XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", true);
	Atom actualType;
	int format = 0;
	unsigned long num = 0, bytes = 0;
	unsigned char *data = NULL;
	Window ewmhWindow = 0;

	int status = XGetWindowProperty(display, window, netSupportingWmCheck,
					0L, 1L, false, XA_WINDOW, &actualType,
					&format, &num, &bytes, &data);

	if (status == Success) {
		if (num > 0) {
			ewmhWindow = ((Window *)data)[0];
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}

	if (ewmhWindow) {
		status = XGetWindowProperty(display, ewmhWindow,
					    netSupportingWmCheck, 0L, 1L, false,
					    XA_WINDOW, &actualType, &format,
					    &num, &bytes, &data);
		if (status != Success || num == 0 ||
		    ewmhWindow != ((Window *)data)[0]) {
			ewmhWindow = 0;
		}
		if (status == Success && data) {
			XFree(data);
		}
	}
	return ewmhWindow != 0;
}

static QStringList getStates(Window window)
{
	QStringList states;
	if (!window || !ewmhIsSupported()) {
		return states;
	}

	Atom wmState = XInternAtom(disp(), "_NET_WM_STATE", true), type;
	int format;
	unsigned long num, bytes;
	unsigned char *data;

	int status = XGetWindowProperty(disp(), window, wmState, 0, ~0L, false,
					AnyPropertyType, &type, &format, &num,
					&bytes, &data);

	if (status == Success) {
		for (unsigned long i = 0; i < num; i++) {
			states.append(QString(
				XGetAtomName(disp(), ((Atom *)data)[i])));
		}
		XFree(data);
	}

	return states;
}

static std::vector<Window> getTopLevelWindows()
{
	std::vector<Window> res;
	if (!ewmhIsSupported()) {
		return res;
	}

	Atom netClList = XInternAtom(disp(), "_NET_CLIENT_LIST", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window *data = 0;

	for (int i = 0; i < ScreenCount(disp()); ++i) {
		Window rootWindow = RootWindow(disp(), i);
		if (!rootWindow) {
			continue;
		}

		int status = XGetWindowProperty(disp(), rootWindow, netClList,
						0L, ~0L, false, AnyPropertyType,
						&actualType, &format, &num,
						&bytes, (uint8_t **)&data);

		if (status != Success) {
			continue;
		}

		for (unsigned long i = 0; i < num; ++i) {
			res.emplace_back(data[i]);
		}

		XFree(data);
	}

	return res;
}

std::string getWindowName(Window window)
{
	auto display = disp();
	if (!display || !window) {
		return "";
	}
	std::string windowTitle;
	char *name;
	int status = XFetchName(display, window, &name);
	if (status >= Success && name != nullptr) {
		std::string str(name);
		windowTitle = str;
		XFree(name);
	} else {
		XTextProperty xtp_new_name;
		if (XGetWMName(display, window, &xtp_new_name) != 0 &&
		    xtp_new_name.value != nullptr) {
			std::string str((const char *)xtp_new_name.value);
			windowTitle = str;
			XFree(xtp_new_name.value);
		}
	}

	return windowTitle;
}

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);
	for (auto window : getTopLevelWindows()) {
		auto name = getWindowName(window);
		if (name.empty()) {
			continue;
		}
		windows.emplace_back(name);
	}
}

void GetWindowList(QStringList &windows)
{
	windows.clear();
	for (auto window : getTopLevelWindows()) {
		auto name = getWindowName(window);
		if (name.empty()) {
			continue;
		}
		windows << QString::fromStdString(name);
	}
}

int getActiveWindow(Window *&window)
{
	if (!ewmhIsSupported()) {
		return -1;
	}

	Atom active = XInternAtom(disp(), "_NET_ACTIVE_WINDOW", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;

	auto rootWindow = DefaultRootWindow(disp());
	if (!rootWindow) {
		return -2;
	}

	return XGetWindowProperty(disp(), rootWindow, active, 0L, ~0L, false,
				  AnyPropertyType, &actualType, &format, &num,
				  &bytes, (uint8_t **)&window);
}

void GetCurrentWindowTitle(std::string &title)
{
	Window *data = 0;
	if (getActiveWindow(data) != Success || !data) {
		return;
	}
	if (!data[0]) {
		XFree(data);
		return;
	}

	auto name = getWindowName(data[0]);
	XFree(data);
	if (name.empty()) {
		return;
	}
	title = name;
}

bool windowStatesAreSet(const std::string &windowTitle,
			std::vector<QString> &expectedStates)
{
	if (!ewmhIsSupported()) {
		return false;
	}

	std::vector<Window> windows = getTopLevelWindows();
	for (auto &window : windows) {
		auto name = getWindowName(window);
		if (name.empty()) {
			continue;
		}

		bool equals = windowTitle == name;
		bool matches = QString::fromStdString(name).contains(
			QRegularExpression(
				QString::fromStdString(windowTitle)));

		if (!(equals || matches)) {
			continue;
		}

		QStringList states = getStates(window);
		if (states.isEmpty()) {
			if (expectedStates.empty()) {
				return true;
			}
			return false;
		}

		for (const auto &state : expectedStates) {
			if (!states.contains(state)) {
				return false;
			}
		}
		return true;
	}
	return false;
}

bool IsMaximized(const std::string &title)
{
	std::vector<QString> states;
	states.emplace_back("_NET_WM_STATE_MAXIMIZED_VERT");
	states.emplace_back("_NET_WM_STATE_MAXIMIZED_HORZ");
	return windowStatesAreSet(title, states);
}

bool IsFullscreen(const std::string &title)
{
	std::vector<QString> states;
	states.emplace_back("_NET_WM_STATE_FULLSCREEN");
	return windowStatesAreSet(title, states);
}

std::optional<std::string> GetTextInWindow(const std::string &window)
{
	// Not implemented
	return {};
}

void GetProcessList(QStringList &processes)
{
	processes.clear();
}

long getForegroundProcessPid()
{
	Window *window;
	if (getActiveWindow(window) != Success || !window || !*window) {
		return -1;
	}
	Atom atom, actual_type;
	int actual_format;
	unsigned long nitems;
	unsigned long bytes_after;
	unsigned char *prop;
	long long pid;
	atom = XInternAtom(disp(), "_NET_WM_PID", True);
	auto status = XGetWindowProperty(disp(), *window, atom, 0, 1024, False,
					 XA_CARDINAL, &actual_type,
					 &actual_format, &nitems, &bytes_after,
					 &prop);
	XFree(window);
	if (status != 0) {
		return -2;
	}
	if (!prop) {
		return -3;
	}

	pid = *((long *)prop);
	XFree(prop);
	return pid;
}

std::string getProcNameFromPid(long pid)
{
	std::string path = "/proc/" + std::to_string(pid) + "/comm";
	std::ifstream t(path);
	std::stringstream buffer;
	buffer << t.rdbuf();
	std::string name = buffer.str();
	if (!name.empty() && name[name.length() - 1] == '\n') {
		name.erase(name.length() - 1);
	}
	return name;
}

void GetForegroundProcessName(QString &proc)
{
	std::string temp;
	GetForegroundProcessName(temp);
	proc = QString::fromStdString(temp);
}

void GetForegroundProcessName(std::string &proc)
{
	proc.resize(0);
	auto pid = getForegroundProcessPid();
	proc = getProcNameFromPid(pid);
}

bool IsInFocus(const QString &executable)
{
	std::string current;
	GetForegroundProcessName(current);

	// True if executable switch equals current window
	bool equals = (executable.toStdString() == current);
	// True if executable switch matches current window
	bool matches = QString::fromStdString(current).contains(
		QRegularExpression(executable));

	return (equals || matches);
}

int SecondsSinceLastInput()
{
	if (!canGetIdleTime) {
		return 0;
	}

	auto display = disp();
	if (!display) {
		return 0;
	}

	auto window = DefaultRootWindow(display);
	if (!window) {
		return 0;
	}

	time_t idle_time;
	auto mit_info = allocSSFunc();

	auto status = querySSFunc(display, window, mit_info);
	idle_time = (mit_info->idle) / 1000;
	XFree(mit_info);

	return status != 0 ? idle_time : 0;
}

static std::unordered_map<HotkeyType, long> keyTable = {
	// Chars
	{HotkeyType::Key_A, XK_A},
	{HotkeyType::Key_B, XK_B},
	{HotkeyType::Key_C, XK_C},
	{HotkeyType::Key_D, XK_D},
	{HotkeyType::Key_E, XK_E},
	{HotkeyType::Key_F, XK_F},
	{HotkeyType::Key_G, XK_G},
	{HotkeyType::Key_H, XK_H},
	{HotkeyType::Key_I, XK_I},
	{HotkeyType::Key_J, XK_J},
	{HotkeyType::Key_K, XK_K},
	{HotkeyType::Key_L, XK_L},
	{HotkeyType::Key_M, XK_M},
	{HotkeyType::Key_N, XK_N},
	{HotkeyType::Key_O, XK_O},
	{HotkeyType::Key_P, XK_P},
	{HotkeyType::Key_Q, XK_Q},
	{HotkeyType::Key_R, XK_R},
	{HotkeyType::Key_S, XK_S},
	{HotkeyType::Key_T, XK_T},
	{HotkeyType::Key_U, XK_U},
	{HotkeyType::Key_V, XK_V},
	{HotkeyType::Key_W, XK_W},
	{HotkeyType::Key_X, XK_X},
	{HotkeyType::Key_Y, XK_Y},
	{HotkeyType::Key_Z, XK_Z},

	// Numbers
	{HotkeyType::Key_0, XK_0},
	{HotkeyType::Key_1, XK_1},
	{HotkeyType::Key_2, XK_2},
	{HotkeyType::Key_3, XK_3},
	{HotkeyType::Key_4, XK_4},
	{HotkeyType::Key_5, XK_5},
	{HotkeyType::Key_6, XK_6},
	{HotkeyType::Key_7, XK_7},
	{HotkeyType::Key_8, XK_8},
	{HotkeyType::Key_9, XK_9},

	{HotkeyType::Key_F1, XK_F1},
	{HotkeyType::Key_F2, XK_F2},
	{HotkeyType::Key_F3, XK_F3},
	{HotkeyType::Key_F4, XK_F4},
	{HotkeyType::Key_F5, XK_F5},
	{HotkeyType::Key_F6, XK_F6},
	{HotkeyType::Key_F7, XK_F7},
	{HotkeyType::Key_F8, XK_F8},
	{HotkeyType::Key_F9, XK_F9},
	{HotkeyType::Key_F10, XK_F10},
	{HotkeyType::Key_F11, XK_F11},
	{HotkeyType::Key_F12, XK_F12},
	{HotkeyType::Key_F13, XK_F13},
	{HotkeyType::Key_F14, XK_F14},
	{HotkeyType::Key_F15, XK_F15},
	{HotkeyType::Key_F16, XK_F16},
	{HotkeyType::Key_F17, XK_F17},
	{HotkeyType::Key_F18, XK_F18},
	{HotkeyType::Key_F19, XK_F19},
	{HotkeyType::Key_F20, XK_F20},
	{HotkeyType::Key_F21, XK_F21},
	{HotkeyType::Key_F22, XK_F22},
	{HotkeyType::Key_F23, XK_F23},
	{HotkeyType::Key_F24, XK_F24},

	{HotkeyType::Key_Escape, XK_Escape},
	{HotkeyType::Key_Space, XK_space},
	{HotkeyType::Key_Return, XK_Return},
	{HotkeyType::Key_Backspace, XK_BackSpace},
	{HotkeyType::Key_Tab, XK_Tab},

	{HotkeyType::Key_Shift_L, XK_Shift_L},
	{HotkeyType::Key_Shift_R, XK_Shift_R},
	{HotkeyType::Key_Control_L, XK_Control_L},
	{HotkeyType::Key_Control_R, XK_Control_R},
	{HotkeyType::Key_Alt_L, XK_Alt_L},
	{HotkeyType::Key_Alt_R, XK_Alt_R},
	{HotkeyType::Key_Win_L, XK_Super_L},
	{HotkeyType::Key_Win_R, XK_Super_R},
	{HotkeyType::Key_Apps, XK_Hyper_L},

	{HotkeyType::Key_CapsLock, XK_Caps_Lock},
	{HotkeyType::Key_NumLock, XK_Num_Lock},
	{HotkeyType::Key_ScrollLock, XK_Scroll_Lock},

	{HotkeyType::Key_PrintScreen, XK_Print},
	{HotkeyType::Key_Pause, XK_Pause},

	{HotkeyType::Key_Insert, XK_Insert},
	{HotkeyType::Key_Delete, XK_Delete},
	{HotkeyType::Key_PageUP, XK_Page_Up},
	{HotkeyType::Key_PageDown, XK_Page_Down},
	{HotkeyType::Key_Home, XK_Home},
	{HotkeyType::Key_End, XK_End},

	{HotkeyType::Key_Left, XK_Left},
	{HotkeyType::Key_Up, XK_Up},
	{HotkeyType::Key_Right, XK_Right},
	{HotkeyType::Key_Down, XK_Down},

	{HotkeyType::Key_Numpad0, XK_KP_0},
	{HotkeyType::Key_Numpad1, XK_KP_1},
	{HotkeyType::Key_Numpad2, XK_KP_2},
	{HotkeyType::Key_Numpad3, XK_KP_3},
	{HotkeyType::Key_Numpad4, XK_KP_4},
	{HotkeyType::Key_Numpad5, XK_KP_5},
	{HotkeyType::Key_Numpad6, XK_KP_6},
	{HotkeyType::Key_Numpad7, XK_KP_7},
	{HotkeyType::Key_Numpad8, XK_KP_8},
	{HotkeyType::Key_Numpad9, XK_KP_9},

	{HotkeyType::Key_NumpadAdd, XK_KP_Add},
	{HotkeyType::Key_NumpadSubtract, XK_KP_Subtract},
	{HotkeyType::Key_NumpadMultiply, XK_KP_Multiply},
	{HotkeyType::Key_NumpadDivide, XK_KP_Divide},
	{HotkeyType::Key_NumpadDecimal, XK_KP_Decimal},
	{HotkeyType::Key_NumpadEnter, XK_KP_Enter},
};

void PressKeys(const std::vector<HotkeyType> keys, int duration)
{
	if (!canSimulateKeyPresses) {
		return;
	}

	Display *display = disp();
	if (!display) {
		return;
	}

	// Press keys
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		pressFunc(display, XKeysymToKeycode(display, it->second), true,
			  CurrentTime);
	}
	XFlush(display);

	std::this_thread::sleep_for(std::chrono::milliseconds(duration));

	// Release keys
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		pressFunc(display, XKeysymToKeycode(display, it->second), false,
			  CurrentTime);
	}

	XFlush(display);
}

void PlatformInit()
{
	auto display = disp();
	if (!display) {
		return;
	}

	libXtstHandle = new QLibrary("libXtst", nullptr);
	pressFunc = (keyPressFunc)libXtstHandle->resolve("XTestFakeKeyEvent");
	int _;
	canSimulateKeyPresses = pressFunc &&
				XQueryExtension(disp(), "XTEST", &_, &_, &_);

	libXssHandle = new QLibrary("libXss", nullptr);
	allocSSFunc = (XScreenSaverAllocInfoFunc)libXssHandle->resolve(
		"XScreenSaverAllocInfo");
	querySSFunc = (XScreenSaverQueryInfoFunc)libXssHandle->resolve(
		"XScreenSaverQueryInfo");
	canGetIdleTime = allocSSFunc && querySSFunc &&
			 XQueryExtension(disp(), ScreenSaverName, &_, &_, &_);
}

void PlatformCleanup()
{
	if (libXtstHandle) {
		delete libXtstHandle;
		libXtstHandle = nullptr;
	}
	if (libXssHandle) {
		delete libXssHandle;
		libXssHandle = nullptr;
	}
	cleanupDisplay();
}

} // namespace advss
