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
#include "advanced-scene-switcher.hpp"

using namespace std;

static Display* xdisplay = 0;

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
	Atom netSupportingWmCheck = XInternAtom(display,
			"_NET_SUPPORTING_WM_CHECK", true);
	Atom actualType;
	int format = 0;
	unsigned long num = 0, bytes = 0;
	unsigned char *data = NULL;
	Window ewmh_window = 0;

	int status = XGetWindowProperty(
			display,
			DefaultRootWindow(display),
			netSupportingWmCheck,
			0L,
			1L,
			false,
			XA_WINDOW,
			&actualType,
			&format,
			&num,
			&bytes,
			&data);

	if (status == Success) {
		if (num > 0) {
			ewmh_window = ((Window*)data)[0];
		}
		if (data) {
			XFree(data);
			data = NULL;
		}
	}

	if (ewmh_window) {
		status = XGetWindowProperty(
				display,
				ewmh_window,
				netSupportingWmCheck,
				0L,
				1L,
				false,
				XA_WINDOW,
				&actualType,
				&format,
				&num,
				&bytes,
				&data);
		if (status != Success || num == 0 ||
				ewmh_window != ((Window*)data)[0]) {
			ewmh_window = 0;
		}
		if (status == Success && data) {
			XFree(data);
		}
	}

	return ewmh_window != 0;
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
	Window* data = 0;

	for (int i = 0; i < ScreenCount(disp()); ++i) {
		Window rootWin = RootWindow(disp(), i);

		int status = XGetWindowProperty(
				disp(),
				rootWin,
				netClList,
				0L,
				~0L,
				false,
				AnyPropertyType,
				&actualType,
				&format,
				&num,
				&bytes,
				(uint8_t**)&data);

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
	char* name;

	int status = XFetchName(disp(), w, &name);
	if (status >= Success && name != nullptr)
	{
		std::string str(name);
		windowTitle = str;
	}

	XFree(name);

	return windowTitle;
}

void GetWindowList(vector<string> &windows)
{
	windows.resize(0);

	for (size_t i = 0; i < getTopLevelWindows().size(); ++i){
		if (GetWindowTitle(i) != "")
			windows.emplace_back(GetWindowTitle(i));
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
	Window* data = 0;
	char* name;

	Window rootWin = RootWindow(disp(), 0);

	XGetWindowProperty(
			disp(),
			rootWin,
			active,
			0L,
			~0L,
			false,
			AnyPropertyType,
			&actualType,
			&format,
			&num,
			&bytes,
			(uint8_t**)&data);

	int status = XFetchName(disp(), data[0], &name);

	if (status >= Success && name != nullptr) {
		std::string str(name);
		title = str;
	}

	XFree(name);
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
 
	if(XQueryPointer(dpy, root, &ret_root, &ret_child, &root_x, &root_y,
					 &win_x, &win_y, &mask))
	{   
		pos = pair<int, int> (root_x,root_y);
	}
	XCloseDisplay(dpy);
	return pos;
}

bool isFullscreen()
{
	if (!ewmhIsSupported()) {
		return false;
	}

	Atom active = XInternAtom(disp(), "_NET_ACTIVE_WINDOW", true);
	Atom actualType;
	int format;
	unsigned long num, bytes;
	Window* data = 0;

	Window rootWin = RootWindow(disp(), 0);
	XGetWindowProperty(
			disp(),
			rootWin,
			active,
			0L,
			~0L,
			false,
			AnyPropertyType,
			&actualType,
			&format,
			&num,
			&bytes,
			(uint8_t**)&data);


	XWindowAttributes window_attributes_return;
	XWindowAttributes screen_attributes_return;

	XGetWindowAttributes(disp(), rootWin, &screen_attributes_return);
	XGetWindowAttributes(disp(), data[0], &window_attributes_return);
	
	//menu bar is always 24 pixels in height
	return (window_attributes_return.width >= screen_attributes_return.width &&
	window_attributes_return.height + 24 >= screen_attributes_return.height) ? true : false; 
}

//exe switch is not quite what is expected but it works for now
void GetProcessList(QStringList &processes) 
{
	processes.clear();
	for (size_t i = 0; i < getTopLevelWindows().size(); ++i){
		string s = GetWindowTitle(i);
		if (s != "")
			processes << QString::fromStdString(s);
	}
}

bool isInFocus(const QString &exeToCheck) 
{
	string curWindow;
	GetCurrentWindowTitle(curWindow);

        return (QString::compare(
		QString::fromStdString(curWindow), 
		exeToCheck, 
		Qt::CaseInsensitive) == 0) ? true : false;
}


int secondsSinceLastInput()
{
        time_t idle_time;
        static XScreenSaverInfo *mit_info;
        Display *display;
        int screen;

        mit_info = XScreenSaverAllocInfo();

        if((display=XOpenDisplay(NULL)) == NULL) 
	{ 
		return(-1); 
	}
        screen = DefaultScreen(display);
        XScreenSaverQueryInfo(display, RootWindow(display,screen), mit_info);
        idle_time = (mit_info->idle) / 1000;
        XFree(mit_info);
        XCloseDisplay(display);
 
        return idle_time;
}


