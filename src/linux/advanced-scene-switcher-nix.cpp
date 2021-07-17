#include "../headers/hotkey.hpp"

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

static Display *xdisplay = 0;

static QLibrary *libXtstHandle = nullptr;
typedef int (*keyPressFunc)(Display *, unsigned int, bool, unsigned long);
static keyPressFunc pressFunc = nullptr;
bool canSimulateKeyPresses = false;

Display *disp()
{
	if (!xdisplay)
		xdisplay = XOpenDisplay(NULL);

	return xdisplay;
}

void cleanupDisplay()
{
	if (!xdisplay)
		return;

	XCloseDisplay(xdisplay);
	xdisplay = 0;
}

static bool ewmhIsSupported()
{
	Display *display = disp();
	Atom netSupportingWmCheck =
		XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", true);
	Atom actualType;
	int format = 0;
	unsigned long num = 0, bytes = 0;
	unsigned char *data = NULL;
	Window ewmh_window = 0;

	int status = XGetWindowProperty(display, DefaultRootWindow(display),
					netSupportingWmCheck, 0L, 1L, false,
					XA_WINDOW, &actualType, &format, &num,
					&bytes, &data);

	if (status == Success) {
		if (num > 0) {
			ewmh_window = ((Window *)data)[0];
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}

	if (ewmh_window) {
		status = XGetWindowProperty(display, ewmh_window,
					    netSupportingWmCheck, 0L, 1L, false,
					    XA_WINDOW, &actualType, &format,
					    &num, &bytes, &data);
		if (status != Success || num == 0 ||
		    ewmh_window != ((Window *)data)[0]) {
			ewmh_window = 0;
		}
		if (status == Success && data) {
			XFree(data);
		}
	}

	return ewmh_window != 0;
}

static QStringList getStates(Window window)
{
	QStringList states;

	if (!ewmhIsSupported())
		return states;

	Atom wmState = XInternAtom(disp(), "_NET_WM_STATE", true), type;
	int format;
	unsigned long num, bytes;
	unsigned char *data;

	int status = XGetWindowProperty(disp(), window, wmState, 0, ~0L, false,
					AnyPropertyType, &type, &format, &num,
					&bytes, &data);

	if (status == Success)
		for (unsigned long i = 0; i < num; i++)
			states.append(QString(
				XGetAtomName(disp(), ((Atom *)data)[i])));

	return states;
}

static std::vector<Window> getTopLevelWindows()
{
	std::vector<Window> res;

	res.resize(0);

	if (!ewmhIsSupported()) {
		return res;
	}

	Atom netClList = XInternAtom(disp(), "_NET_CLIENT_LIST", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window *data = 0;

	for (int i = 0; i < ScreenCount(disp()); ++i) {
		Window rootWin = RootWindow(disp(), i);

		int status = XGetWindowProperty(disp(), rootWin, netClList, 0L,
						~0L, false, AnyPropertyType,
						&actualType, &format, &num,
						&bytes, (uint8_t **)&data);

		if (status != Success) {
			continue;
		}

		for (unsigned long i = 0; i < num; ++i)
			res.emplace_back(data[i]);

		XFree(data);
	}

	return res;
}

static std::string GetWindowTitle(size_t i)
{
	Window w = getTopLevelWindows().at(i);
	std::string windowTitle;
	char *name;

	XTextProperty text;
	int status = XGetTextProperty(
		disp(), w, &text, XInternAtom(disp(), "_NET_WM_NAME", true));
	if (status == 0)
		status = XGetTextProperty(disp(), w, &text,
					  XInternAtom(disp(), "WM_NAME", true));
	name = reinterpret_cast<char *>(text.value);

	if (status != 0 && name != nullptr) {
		std::string str(name);
		windowTitle = str;
		XFree(name);
	}

	return windowTitle;
}

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);

	for (size_t i = 0; i < getTopLevelWindows().size(); ++i) {
		if (GetWindowTitle(i) != "")
			windows.emplace_back(GetWindowTitle(i));
	}
}

// Overloaded
void GetWindowList(QStringList &windows)
{
	windows.clear();

	for (size_t i = 0; i < getTopLevelWindows().size(); ++i) {
		if (GetWindowTitle(i) != "")
			windows << QString::fromStdString(GetWindowTitle(i));
	}
}

void GetCurrentWindowTitle(std::string &title)
{
	if (!ewmhIsSupported()) {
		return;
	}

	Atom active = XInternAtom(disp(), "_NET_ACTIVE_WINDOW", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window *data = 0;
	char *name;

	Window rootWin = RootWindow(disp(), 0);

	int xstatus = XGetWindowProperty(disp(), rootWin, active, 0L, ~0L,
					 false, AnyPropertyType, &actualType,
					 &format, &num, &bytes,
					 (uint8_t **)&data);

	if (data == nullptr) {
		return;
	}

	int status = 0;
	XTextProperty text;
	if (xstatus == Success && data[0]) {
		status = XGetTextProperty(disp(), data[0], &text,
					  XInternAtom(disp(), "_NET_WM_NAME",
						      true));
		if (status == 0)
			status = XGetTextProperty(disp(), data[0], &text,
						  XInternAtom(disp(), "WM_NAME",
							      true));
	}
	name = reinterpret_cast<char *>(text.value);

	if (status != 0 && name != nullptr) {
		std::string str(name);
		title = str;
		XFree(name);
	}
}

std::pair<int, int> getCursorPos()
{
	std::pair<int, int> pos(0, 0);
	Display *dpy;
	Window root;
	Window ret_root;
	Window ret_child;
	int root_x;
	int root_y;
	int win_x;
	int win_y;
	unsigned int mask;

	dpy = disp();
	root = XDefaultRootWindow(dpy);

	if (XQueryPointer(dpy, root, &ret_root, &ret_child, &root_x, &root_y,
			  &win_x, &win_y, &mask)) {
		pos = std::pair<int, int>(root_x, root_y);
	}
	return pos;
}

bool isMaximized(const std::string &title)
{
	if (!ewmhIsSupported())
		return false;

	// Find switch in top level windows
	std::vector<Window> windows = getTopLevelWindows();
	for (auto &window : windows) {
		XTextProperty text;
		int status = XGetTextProperty(
			disp(), window, &text,
			XInternAtom(disp(), "_NET_WM_NAME", true));
		if (status == 0)
			status = XGetTextProperty(disp(), window, &text,
						  XInternAtom(disp(), "WM_NAME",
							      true));
		char *name = reinterpret_cast<char *>(text.value);

		if (status == 0 || name == nullptr)
			continue;

		// True if switch equals window
		bool equals = (title == name);
		// True if switch matches window
		bool matches = QString::fromStdString(name).contains(
			QRegularExpression(QString::fromStdString(title)));

		// If found, check if switch is maximized
		if (equals || matches) {
			QStringList states = getStates(window);

			if (!states.isEmpty()) {
				// True if window is maximized vertically
				bool vertical = states.contains(
					"_NET_WM_STATE_MAXIMIZED_VERT");
				// True if window is maximized horizontally
				bool horizontal = states.contains(
					"_NET_WM_STATE_MAXIMIZED_HORZ");

				return (vertical && horizontal);
			}

			break;
		}
	}

	return false;
}

bool isFullscreen(const std::string &title)
{
	if (!ewmhIsSupported())
		return false;

	// Find switch in top level windows
	std::vector<Window> windows = getTopLevelWindows();
	for (auto &window : windows) {
		XTextProperty text;
		int status = XGetTextProperty(
			disp(), window, &text,
			XInternAtom(disp(), "_NET_WM_NAME", true));
		if (status == 0)
			status = XGetTextProperty(disp(), window, &text,
						  XInternAtom(disp(), "WM_NAME",
							      true));
		char *name = reinterpret_cast<char *>(text.value);

		if (status == 0 || name == nullptr)
			continue;

		// True if switch equals window
		bool equals = (title == name);
		// True if switch matches window
		bool matches = QString::fromStdString(name).contains(
			QRegularExpression(QString::fromStdString(title)));

		// If found, check if switch is fullscreen
		if (equals || matches) {
			QStringList states = getStates(window);

			if (!states.isEmpty()) {
				// True if window is fullscreen
				bool fullscreen = states.contains(
					"_NET_WM_STATE_FULLSCREEN");

				return (fullscreen);
			}

			break;
		}
	}

	return false;
}

//exe switch is not quite what is expected but it works for now
void GetProcessList(QStringList &processes)
{
	processes.clear();
	for (size_t i = 0; i < getTopLevelWindows().size(); ++i) {
		std::string s = GetWindowTitle(i);
		if (s != "")
			processes << QString::fromStdString(s);
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

int secondsSinceLastInput()
{
	time_t idle_time;
	static XScreenSaverInfo *mit_info;
	Display *display;
	int screen;

	mit_info = XScreenSaverAllocInfo();

	if ((display = disp()) == NULL) {
		return -1;
	}
	screen = DefaultScreen(display);
	XScreenSaverQueryInfo(display, RootWindow(display, screen), mit_info);
	idle_time = (mit_info->idle) / 1000;
	XFree(mit_info);

	return idle_time;
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

void PressKeys(const std::vector<HotkeyType> keys)
{
	if (!canSimulateKeyPresses) {
		return;
	}

	Display *display = disp();
	if (display == NULL)
		return;

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

	std::this_thread::sleep_for(std::chrono::milliseconds(300));

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
	libXtstHandle = new QLibrary("libXtst.so", nullptr);
	pressFunc = (keyPressFunc)libXtstHandle->resolve("XTestFakeKeyEvent");
	int _;
	canSimulateKeyPresses = pressFunc &&
				!XQueryExtension(disp(), "XTEST", &_, &_, &_);
}

void PlatformCleanup()
{
	delete libXtstHandle;
	libXtstHandle = nullptr;

	cleanupDisplay();
}
