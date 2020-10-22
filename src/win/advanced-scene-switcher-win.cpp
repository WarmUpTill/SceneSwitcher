#include <windows.h>
#include <util/platform.h>
#include "../headers/advanced-scene-switcher.hpp"
#include <TlHelp32.h>
#include <Psapi.h>

using namespace std;

static bool GetWindowTitle(HWND window, string &title)
{
	size_t len = (size_t)GetWindowTextLengthW(window);
	wstring wtitle;

	wtitle.resize(len);
	if (!GetWindowTextW(window, &wtitle[0], (int)len + 1))
		return false;

	len = os_wcs_to_utf8(wtitle.c_str(), 0, nullptr, 0);
	title.resize(len);
	os_wcs_to_utf8(wtitle.c_str(), 0, &title[0], len + 1);
	return true;
}

static bool WindowValid(HWND window)
{
	LONG_PTR styles;
	DWORD id;

	if (!IsWindowVisible(window))
		return false;
	GetWindowThreadProcessId(window, &id);
	if (id == GetCurrentProcessId())
		return false;

	styles = GetWindowLongPtr(window, GWL_STYLE);

	if (styles & WS_CHILD)
		return false;

	return true;
}

void GetWindowList(vector<string> &windows)
{
	windows.resize(0);

	HWND window = GetWindow(GetDesktopWindow(), GW_CHILD);

	while (window) {
		string title;
		if (WindowValid(window) && GetWindowTitle(window, title))
			windows.emplace_back(title);
		window = GetNextWindow(window, GW_HWNDNEXT);
	}
}

// Overloaded
void GetWindowList(QStringList &windows)
{
	windows.clear();

	HWND window = GetWindow(GetDesktopWindow(), GW_CHILD);

	while (window) {
		string title;
		if (WindowValid(window) && GetWindowTitle(window, title))
			windows << QString::fromStdString(title);
		window = GetNextWindow(window, GW_HWNDNEXT);
	}
}

void GetCurrentWindowTitle(string &title)
{
	HWND window = GetForegroundWindow();
	DWORD pid;
	DWORD thid;
	thid = GetWindowThreadProcessId(window, &pid);
	/*GetWindowText will freeze if the control it is reading was created in another thread.
	It does not directly read the control.Instead,
	it waits for the thread that created the control to process a WM_GETTEXT message.
	So if that thread is frozen in a WaitFor... call you have a deadlock.*/
	if (GetCurrentProcessId() == pid) {
		title = "OBS";
		return;
	}
	GetWindowTitle(window, title);
}

std::pair<int, int> getCursorPos()
{
	std::pair<int, int> pos(0, 0);
	POINT cursorPos;
	if (GetPhysicalCursorPos(&cursorPos)) {
		pos.first = cursorPos.x;
		pos.second = cursorPos.y;
	}
	return pos;
}

bool isMaximized(std::string &title)
{
	RECT appBounds;
	MONITORINFO monitorInfo = {0};

	HWND hwnd = GetWindow(GetDesktopWindow(), GW_CHILD);
	while (hwnd) {
		string wtitle;
		if (WindowValid(hwnd) && GetWindowTitle(hwnd, wtitle) &&
		    (wtitle == title ||
		     QString::fromStdString(wtitle).contains(QRegularExpression(
			     QString::fromStdString(title)))))
			break;

		hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
	}

	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
		       &monitorInfo);

	if (hwnd && hwnd != GetDesktopWindow() && hwnd != GetShellWindow()) {
		if (IsZoomed(hwnd)) {
			return true;
		}
		GetWindowRect(hwnd, &appBounds);
		if (monitorInfo.rcMonitor.bottom == appBounds.bottom &&
		    monitorInfo.rcMonitor.top == appBounds.top &&
		    monitorInfo.rcMonitor.left == appBounds.left &&
		    monitorInfo.rcMonitor.right == appBounds.right) {
			return true;
		}
	}
	return false;
}

bool isFullscreen(std::string &title)
{
	RECT appBounds;
	MONITORINFO monitorInfo = {0};

	HWND hwnd = GetWindow(GetDesktopWindow(), GW_CHILD);
	while (hwnd) {
		string wtitle;
		if (WindowValid(hwnd) && GetWindowTitle(hwnd, wtitle) &&
		    (wtitle == title ||
		     QString::fromStdString(wtitle).contains(QRegularExpression(
			     QString::fromStdString(title)))))
			break;

		hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
	}

	monitorInfo.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST),
		       &monitorInfo);

	if (hwnd && hwnd != GetDesktopWindow() && hwnd != GetShellWindow()) {
		GetWindowRect(hwnd, &appBounds);
		if (monitorInfo.rcMonitor.bottom == appBounds.bottom &&
		    monitorInfo.rcMonitor.top == appBounds.top &&
		    monitorInfo.rcMonitor.left == appBounds.left &&
		    monitorInfo.rcMonitor.right == appBounds.right) {
			return true;
		}
	}

	return false;
}

void GetProcessList(QStringList &processes)
{

	HANDLE procSnapshot;
	PROCESSENTRY32 procEntry;

	procSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (procSnapshot == INVALID_HANDLE_VALUE)
		return;

	procEntry.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(procSnapshot, &procEntry)) {
		CloseHandle(procSnapshot);
		return;
	}

	do {
		QString tempexe = QString::fromWCharArray(procEntry.szExeFile);
		if (tempexe == "System")
			continue;
		if (tempexe == "[System Process]")
			continue;
		if (processes.contains(tempexe))
			continue;
		processes.append(tempexe);
	} while (Process32Next(procSnapshot, &procEntry));

	CloseHandle(procSnapshot);
}

bool isInFocus(const QString &executable)
{
	// only checks if the current foreground window is from the same executable,
	// may return true for any window from a program
	HWND foregroundWindow = GetForegroundWindow();
	DWORD processId = 0;
	GetWindowThreadProcessId(foregroundWindow, &processId);

	HANDLE process = OpenProcess(
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (process == NULL)
		return false;

	WCHAR executablePath[600];
	GetModuleFileNameEx(process, 0, executablePath, 600);
	CloseHandle(process);

	QString file = QString::fromWCharArray(executablePath)
			       .split(QRegularExpression("(/|\\\\)"))
			       .back();

	// True if executable switch equals current window
	bool equals = (executable == file);
	// True if executable switch matches current window
	bool matches = file.contains(QRegularExpression(executable));

	return (equals || matches);
}

int getLastInputTime()
{
	LASTINPUTINFO lastInputInfo;
	lastInputInfo.cbSize = sizeof(LASTINPUTINFO);
	if (GetLastInputInfo(&lastInputInfo))
		return lastInputInfo.dwTime;
	return 0;
}

int getTime()
{
	return GetTickCount();
}

int secondsSinceLastInput()
{
	return (getTime() - getLastInputTime()) / 1000;
}
