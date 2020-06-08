#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
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
#include "../headers/advanced-scene-switcher.hpp"

using namespace std;

static Display *xdisplay = 0;

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

void GetWindowList(vector<string> &windows)
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

void GetCurrentWindowTitle(string &title)
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

	int status = 0;
	XTextProperty text;
	if (xstatus == Success) {
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

pair<int, int> getCursorPos()
{
	pair<int, int> pos(0, 0);
	Display *dpy;
	Window root;
	Window ret_root;
	Window ret_child;
	int root_x;
	int root_y;
	int win_x;
	int win_y;
	unsigned int mask;

	dpy = XOpenDisplay(NULL);
	root = XDefaultRootWindow(dpy);

	if (XQueryPointer(dpy, root, &ret_root, &ret_child, &root_x, &root_y,
			  &win_x, &win_y, &mask)) {
		pos = pair<int, int>(root_x, root_y);
	}
	XCloseDisplay(dpy);
	return pos;
}

bool isFullscreen(std::string &title)
{
	if (!ewmhIsSupported())
		return false;

	// Find switch in top level windows
	vector<Window> windows = getTopLevelWindows();
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
			QRegularExpression(title.c_str()));

		// If found, check if switch is fullscreen
		if (equals || matches) {
			QStringList states = getStates(window);

			if (!states.isEmpty()) {
				// True if window is fullscreen
				bool fullscreen = states.contains(
					"_NET_WM_STATE_FULLSCREEN");
				// True if window is maximized vertically
				bool vertical = states.contains(
					"_NET_WM_STATE_MAXIMIZED_VERT");
				// True if window is maximized horizontally
				bool horizontal = states.contains(
					"_NET_WM_STATE_MAXIMIZED_HORZ");

				return (fullscreen || (vertical && horizontal));
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
		string s = GetWindowTitle(i);
		if (s != "")
			processes << QString::fromStdString(s);
	}
}

bool isInFocus(const QString &executable)
{
	string current;
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

	if ((display = XOpenDisplay(NULL)) == NULL) {
		return (-1);
	}
	screen = DefaultScreen(display);
	XScreenSaverQueryInfo(display, RootWindow(display, screen), mit_info);
	idle_time = (mit_info->idle) / 1000;
	XFree(mit_info);
	XCloseDisplay(display);

	return idle_time;
}
