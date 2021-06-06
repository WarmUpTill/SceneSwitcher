#include "../headers/hotkey.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <util/platform.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <locale>
#include <codecvt>
#include <string>
#include <vector>
#include <QStringList>
#include <QRegularExpression>

#define MAX_SEARCH 1000

static bool GetWindowTitle(HWND window, std::string &title)
{
	size_t len = (size_t)GetWindowTextLengthW(window);
	std::wstring wtitle;

	wtitle.resize(len);
	if (!GetWindowTextW(window, &wtitle[0], (int)len + 1)) {
		return false;
	}

	len = os_wcs_to_utf8(wtitle.c_str(), 0, nullptr, 0);
	title.resize(len);
	os_wcs_to_utf8(wtitle.c_str(), 0, &title[0], len + 1);
	return true;
}

static bool WindowValid(HWND window)
{
	LONG_PTR styles;
	DWORD id;

	if (!IsWindowVisible(window)) {
		return false;
	}
	GetWindowThreadProcessId(window, &id);
	if (id == GetCurrentProcessId()) {
		return false;
	}

	styles = GetWindowLongPtr(window, GWL_STYLE);

	if (styles & WS_CHILD) {
		return false;
	}

	return true;
}

BOOL CALLBACK GetTitleCB(HWND hwnd, LPARAM lParam)
{
	if (!WindowValid(hwnd)) {
		return TRUE;
	}

	std::string title;
	GetWindowTitle(hwnd, title);
	if (title.empty()) {
		return TRUE;
	}

	std::vector<std::string> &titles =
		*reinterpret_cast<std::vector<std::string> *>(lParam);
	titles.push_back(title);

	return TRUE;
}

VOID EnumWindowsWithMetro(__in WNDENUMPROC lpEnumFunc, __in LPARAM lParam)
{
	HWND childWindow = NULL;
	int i = 0;

	while (i < MAX_SEARCH &&
	       (childWindow = FindWindowEx(NULL, childWindow, NULL, NULL))) {
		if (!lpEnumFunc(childWindow, lParam)) {
			return;
		}
		i++;
	}
}

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);
	EnumWindowsWithMetro(GetTitleCB, reinterpret_cast<LPARAM>(&windows));
}

void GetWindowList(QStringList &windows)
{
	windows.clear();

	std::vector<std::string> w;
	GetWindowList(w);
	for (auto window : w) {
		windows << QString::fromStdString(window);
	}

	// Add entry for OBS Studio itself, see GetCurrentWindowTitle
	windows << QString("OBS");
}

void GetCurrentWindowTitle(std::string &title)
{
	HWND window = GetForegroundWindow();
	DWORD pid;
	DWORD thid;
	thid = GetWindowThreadProcessId(window, &pid);
	// GetWindowText will freeze if the control it is reading was created in another thread.
	// It does not directly read the control.
	// Instead it waits for the thread that created the control to process a WM_GETTEXT message.
	// So if that thread is frozen in a WaitFor... call you have a deadlock.
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

HWND getHWNDfromTitle(std::string title)
{
	HWND hwnd = NULL;

	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::wstring wTitle = converter.from_bytes(title);

	hwnd = FindWindowEx(NULL, NULL, NULL, wTitle.c_str());
	return hwnd;
}

bool isMaximized(const std::string &title)
{
	RECT appBounds;
	MONITORINFO monitorInfo = {0};
	HWND hwnd = NULL;

	hwnd = getHWNDfromTitle(title);
	if (!hwnd) {
		return false;
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

bool isFullscreen(const std::string &title)
{
	RECT appBounds;
	MONITORINFO monitorInfo = {0};

	HWND hwnd = NULL;
	hwnd = getHWNDfromTitle(title);

	if (!hwnd) {
		return false;
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
	if (procSnapshot == INVALID_HANDLE_VALUE) {
		return;
	}

	procEntry.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(procSnapshot, &procEntry)) {
		CloseHandle(procSnapshot);
		return;
	}

	do {
		QString tempexe = QString::fromWCharArray(procEntry.szExeFile);
		if (tempexe == "System") {
			continue;
		}
		if (tempexe == "[System Process]") {
			continue;
		}
		if (processes.contains(tempexe)) {
			continue;
		}
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
	if (process == NULL) {
		return false;
	}

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

static std::unordered_map<HotkeyType, long> keyTable = {
	// Chars
	{HotkeyType::Key_A, 0x41},
	{HotkeyType::Key_B, 0x42},
	{HotkeyType::Key_C, 0x43},
	{HotkeyType::Key_D, 0x44},
	{HotkeyType::Key_E, 0x45},
	{HotkeyType::Key_F, 0x46},
	{HotkeyType::Key_G, 0x47},
	{HotkeyType::Key_H, 0x48},
	{HotkeyType::Key_I, 0x49},
	{HotkeyType::Key_J, 0x4A},
	{HotkeyType::Key_K, 0x4B},
	{HotkeyType::Key_L, 0x4C},
	{HotkeyType::Key_M, 0x4D},
	{HotkeyType::Key_N, 0x4E},
	{HotkeyType::Key_O, 0x4F},
	{HotkeyType::Key_P, 0x50},
	{HotkeyType::Key_Q, 0x51},
	{HotkeyType::Key_R, 0x52},
	{HotkeyType::Key_S, 0x53},
	{HotkeyType::Key_T, 0x54},
	{HotkeyType::Key_U, 0x55},
	{HotkeyType::Key_V, 0x56},
	{HotkeyType::Key_W, 0x57},
	{HotkeyType::Key_X, 0x58},
	{HotkeyType::Key_Y, 0x59},
	{HotkeyType::Key_Z, 0x5A},

	// Numbers
	{HotkeyType::Key_0, 0x30},
	{HotkeyType::Key_1, 0x31},
	{HotkeyType::Key_2, 0x32},
	{HotkeyType::Key_3, 0x33},
	{HotkeyType::Key_4, 0x34},
	{HotkeyType::Key_5, 0x35},
	{HotkeyType::Key_6, 0x36},
	{HotkeyType::Key_7, 0x37},
	{HotkeyType::Key_8, 0x38},
	{HotkeyType::Key_9, 0x39},

	{HotkeyType::Key_F1, VK_F1},
	{HotkeyType::Key_F2, VK_F2},
	{HotkeyType::Key_F3, VK_F3},
	{HotkeyType::Key_F4, VK_F4},
	{HotkeyType::Key_F5, VK_F5},
	{HotkeyType::Key_F6, VK_F6},
	{HotkeyType::Key_F7, VK_F7},
	{HotkeyType::Key_F8, VK_F8},
	{HotkeyType::Key_F9, VK_F9},
	{HotkeyType::Key_F10, VK_F10},
	{HotkeyType::Key_F11, VK_F11},
	{HotkeyType::Key_F12, VK_F12},
	{HotkeyType::Key_F13, VK_F13},
	{HotkeyType::Key_F14, VK_F14},
	{HotkeyType::Key_F15, VK_F15},
	{HotkeyType::Key_F16, VK_F16},
	{HotkeyType::Key_F17, VK_F17},
	{HotkeyType::Key_F18, VK_F18},
	{HotkeyType::Key_F19, VK_F19},
	{HotkeyType::Key_F20, VK_F20},
	{HotkeyType::Key_F21, VK_F21},
	{HotkeyType::Key_F22, VK_F22},
	{HotkeyType::Key_F23, VK_F23},
	{HotkeyType::Key_F24, VK_F24},

	{HotkeyType::Key_Escape, VK_ESCAPE},
	{HotkeyType::Key_Space, VK_SPACE},
	{HotkeyType::Key_Return, VK_RETURN},
	{HotkeyType::Key_Backspace, VK_BACK},
	{HotkeyType::Key_Tab, VK_TAB},

	{HotkeyType::Key_Shift_L, VK_LSHIFT},
	{HotkeyType::Key_Shift_R, VK_RSHIFT},
	{HotkeyType::Key_Control_L, VK_LCONTROL},
	{HotkeyType::Key_Control_R, VK_RCONTROL},
	{HotkeyType::Key_Alt_L, VK_LMENU},
	{HotkeyType::Key_Alt_R, VK_RMENU},
	{HotkeyType::Key_Win_L, VK_LWIN},
	{HotkeyType::Key_Win_R, VK_RWIN},
	{HotkeyType::Key_Apps, VK_APPS},

	{HotkeyType::Key_CapsLock, VK_CAPITAL},
	{HotkeyType::Key_NumLock, VK_NUMLOCK},
	{HotkeyType::Key_ScrollLock, VK_SCROLL},

	{HotkeyType::Key_PrintScreen, VK_SNAPSHOT},
	{HotkeyType::Key_Pause, VK_PAUSE},

	{HotkeyType::Key_Insert, VK_INSERT},
	{HotkeyType::Key_Delete, VK_DELETE},
	{HotkeyType::Key_PageUP, VK_PRIOR},
	{HotkeyType::Key_PageDown, VK_NEXT},
	{HotkeyType::Key_Home, VK_HOME},
	{HotkeyType::Key_End, VK_END},

	{HotkeyType::Key_Left, VK_LEFT},
	{HotkeyType::Key_Up, VK_UP},
	{HotkeyType::Key_Right, VK_RIGHT},
	{HotkeyType::Key_Down, VK_DOWN},

	{HotkeyType::Key_Numpad0, VK_NUMPAD0},
	{HotkeyType::Key_Numpad1, VK_NUMPAD1},
	{HotkeyType::Key_Numpad2, VK_NUMPAD2},
	{HotkeyType::Key_Numpad3, VK_NUMPAD3},
	{HotkeyType::Key_Numpad4, VK_NUMPAD4},
	{HotkeyType::Key_Numpad5, VK_NUMPAD5},
	{HotkeyType::Key_Numpad6, VK_NUMPAD6},
	{HotkeyType::Key_Numpad7, VK_NUMPAD7},
	{HotkeyType::Key_Numpad8, VK_NUMPAD8},
	{HotkeyType::Key_Numpad9, VK_NUMPAD9},

	{HotkeyType::Key_NumpadAdd, VK_ADD},
	{HotkeyType::Key_NumpadSubtract, VK_SUBTRACT},
	{HotkeyType::Key_NumpadMultiply, VK_MULTIPLY},
	{HotkeyType::Key_NumpadDivide, VK_DIVIDE},
	{HotkeyType::Key_NumpadDecimal, VK_DECIMAL},
	{HotkeyType::Key_NumpadEnter, VK_RETURN},
};

void PressKeys(const std::vector<HotkeyType> keys)
{
	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	// Press keys
	ip.ki.dwFlags = 0;
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		ip.ki.wVk = it->second;
		SendInput(1, &ip, sizeof(INPUT));
	}

	// When instantly releasing the key presses OBS might miss them
	Sleep(300);

	// Release keys
	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	for (auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		ip.ki.wVk = it->second;
		SendInput(1, &ip, sizeof(INPUT));
	}
}

int getLastInputTime()
{
	LASTINPUTINFO lastInputInfo;
	lastInputInfo.cbSize = sizeof(LASTINPUTINFO);
	if (GetLastInputInfo(&lastInputInfo)) {
		return lastInputInfo.dwTime;
	}
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
