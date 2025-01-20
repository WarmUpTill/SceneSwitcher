#include "platform-funcs.hpp"
#include "log-helper.hpp"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
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
#include <vector>
#include <thread>
#include <unordered_map>
#include <QStringList>
#include <QRegularExpression>
#include <QLibrary>
#ifdef PROCPS_AVAILABLE
#include <proc/readproc.h>
#endif
#ifdef PROCPS2_AVAILABLE
#include <libproc2/pids.h>
#endif
#include <fstream>
#include <sstream>

namespace advss {

static Display *xdisplay = 0;

static QLibrary *libXssHandle = nullptr;
typedef XScreenSaverInfo *(*XScreenSaverAllocInfoFunc)();
typedef int (*XScreenSaverQueryInfoFunc)(Display *, Window, XScreenSaverInfo *);
static XScreenSaverAllocInfoFunc allocSSFunc = nullptr;
static XScreenSaverQueryInfoFunc querySSFunc = nullptr;
bool canGetIdleTime = false;

static QLibrary *libprocps = nullptr;
#ifdef PROCPS_AVAILABLE
typedef PROCTAB *(*openproc_func)(int flags);
typedef void (*closeproc_func)(PROCTAB *PT);
typedef proc_t *(*readproc_func)(PROCTAB *__restrict const PT,
				 proc_t *__restrict p);
static openproc_func openproc_ = nullptr;
static closeproc_func closeproc_ = nullptr;
static readproc_func readproc_ = nullptr;
#endif
static bool libprocpsSupported = false;

static QLibrary *libproc2 = nullptr;
#ifdef PROCPS2_AVAILABLE
typedef int (*procps_pids_new_func)(struct pids_info **info,
				    enum pids_item *items, int numitems);
typedef struct pids_stack *(*procps_pids_get_func)(struct pids_info *info,
						   enum pids_fetch_type which);
typedef int (*procps_pids_unref_func)(struct pids_info **info);
static procps_pids_new_func procps_pids_new_ = nullptr;
static procps_pids_get_func procps_pids_get_ = nullptr;
static procps_pids_unref_func procps_pids_unref_ = nullptr;
#endif
static bool libprocps2Supported = false;

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

std::optional<std::string> GetTextInWindow(const std::string &)
{
	// Not implemented
	return {};
}

static void getProcessListProcps(QStringList &processes)
{
#ifdef PROCPS_AVAILABLE
	PROCTAB *proc = openproc_(PROC_FILLSTAT);
	proc_t proc_info;
	memset(&proc_info, 0, sizeof(proc_info));
	while (readproc_(proc, &proc_info) != NULL) {
		QString procName(proc_info.cmd);
		if (!procName.isEmpty() && !processes.contains(procName)) {
			processes << procName;
		}
	}
	closeproc_(proc);
#endif
}

static void getProcessListProcps2(QStringList &processes)
{
#ifdef PROCPS2_AVAILABLE
	struct pids_info *info = NULL;
	struct pids_stack *stack;
	enum pids_item Items[] = {
		PIDS_CMD,
	};

	if (procps_pids_new_(&info, Items, sizeof(Items) / sizeof(Items[0])) <
	    0) {
		return;
	}
	while ((stack = procps_pids_get_(info, PIDS_FETCH_TASKS_ONLY))) {
#ifdef PROCPS2_USE_INFO
		auto cmd = PIDS_VAL(0, str, stack, info);
#else
		auto cmd = PIDS_VAL(0, str, stack);
#endif
		QString procName(cmd);
		if (!procName.isEmpty() && !processes.contains(procName)) {
			processes << procName;
		}
	}
	procps_pids_unref_(&info);
#endif
}

void GetProcessList(QStringList &processes)
{
	processes.clear();
	if (libprocpsSupported) {
		getProcessListProcps(processes);
		return;
	}
	if (libprocps2Supported) {
		getProcessListProcps2(processes);
	}
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

static void initXss()
{
	libXssHandle = new QLibrary("libXss", nullptr);
	allocSSFunc = (XScreenSaverAllocInfoFunc)libXssHandle->resolve(
		"XScreenSaverAllocInfo");
	querySSFunc = (XScreenSaverQueryInfoFunc)libXssHandle->resolve(
		"XScreenSaverQueryInfo");
	int _;
	canGetIdleTime = allocSSFunc && querySSFunc &&
			 XQueryExtension(disp(), ScreenSaverName, &_, &_, &_);
}

static void initProcps()
{
#ifdef PROCPS_AVAILABLE
	libprocps = new QLibrary("libprocps", nullptr);
	openproc_ = (openproc_func)libprocps->resolve("openproc");
	closeproc_ = (closeproc_func)libprocps->resolve("closeproc");
	readproc_ = (readproc_func)libprocps->resolve("readproc");
	if (openproc_ && closeproc_ && readproc_) {
		libprocpsSupported = true;
		blog(LOG_INFO, "libprocps symbols resolved successfully!");
	}
#endif
}

static void initProc2()
{
#ifdef PROCPS2_AVAILABLE
	libproc2 = new QLibrary("libproc2", nullptr);
	procps_pids_new_ =
		(procps_pids_new_func)libproc2->resolve("procps_pids_new");
	procps_pids_get_ =
		(procps_pids_get_func)libproc2->resolve("procps_pids_get");
	procps_pids_unref_ =
		(procps_pids_unref_func)libproc2->resolve("procps_pids_unref");
	if (procps_pids_new_ && procps_pids_get_ && procps_pids_unref_) {
		libprocps2Supported = true;
		blog(LOG_INFO, "libproc2 symbols resolved successfully!");
	}
#endif
}

void PlatformInit()
{
	auto display = disp();
	if (!display) {
		return;
	}

	initXss();
	initProcps();
	initProc2();
}

static void cleanupHelper(QLibrary *lib)
{
	if (lib) {
		delete lib;
		lib = nullptr;
	}
}

void PlatformCleanup()
{
	cleanupHelper(libXssHandle);
	cleanupHelper(libprocps);
	cleanupHelper(libproc2);
	cleanupDisplay();
}

} // namespace advss
