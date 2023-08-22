#include "platform-funcs.hpp"
#include "hotkey.hpp"

#include <windows.h>
#include <UIAutomation.h>
#include <util/platform.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <locale>
#include <codecvt>
#include <string>
#include <vector>
#include <QStringList>
#include <QRegularExpression>
#include <obs-frontend-api.h>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QWidget>
#include <mutex>

namespace advss {

#define MAX_SEARCH 1000

// Hotkeys
bool canSimulateKeyPresses = true;

// Mouse click
class RawMouseInputFilter;
RawMouseInputFilter *mouseInputFilter;
std::chrono::high_resolution_clock::time_point lastMouseLeftClickTime{};
std::chrono::high_resolution_clock::time_point lastMouseMiddleClickTime{};
std::chrono::high_resolution_clock::time_point lastMouseRightClickTime{};

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

static BOOL CALLBACK GetTitleCB(HWND hwnd, LPARAM lParam)
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

static VOID EnumWindowsWithMetro(__in WNDENUMPROC lpEnumFunc,
				 __in LPARAM lParam)
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

// Asynchronously updates the OBS window list and returns the state of the last
// successful update
const std::vector<std::string> getOBSWindows()
{
	struct OBSWindowListHelper {
		std::vector<std::string> windows;
		std::atomic_bool done;
	};
	static OBSWindowListHelper obsWindowListHelper1 = {{}, {true}};
	static OBSWindowListHelper obsWindowListHelper2 = {{}, {false}};
	auto getQtWindowList = [](void *param) {
		auto list = reinterpret_cast<OBSWindowListHelper *>(param);
		for (auto w : QApplication::topLevelWidgets()) {
			auto title = w->windowTitle();
			if (!title.isEmpty()) {
				list->windows.emplace_back(title.toStdString());
			}
		}
		list->done = true;
	};

	static OBSWindowListHelper *lastDoneHelper = &obsWindowListHelper2;
	static OBSWindowListHelper *pendingHelper = &obsWindowListHelper1;
	static std::mutex mutex;

	/* ------------------------------------------------------------------ */

	std::lock_guard<std::mutex> lock(mutex);
	if (pendingHelper->done) { // Check if swap is needed
		auto temp = lastDoneHelper;
		lastDoneHelper = pendingHelper;
		pendingHelper = temp;
		pendingHelper->done = false;
		pendingHelper->windows.clear();
		obs_queue_task(OBS_TASK_UI, getQtWindowList, pendingHelper,
			       false);
	}

	return lastDoneHelper->windows;
}

void GetWindowList(std::vector<std::string> &windows)
{
	windows.resize(0);
	EnumWindowsWithMetro(GetTitleCB, reinterpret_cast<LPARAM>(&windows));

	// Also add OBS windows
	for (const auto &window : getOBSWindows()) {
		if (!window.empty()) {
			windows.emplace_back(window);
		}
	}

	// Add entry for OBS Studio itself - see GetCurrentWindowTitle()
	windows.emplace_back("OBS");
}

void GetWindowList(QStringList &windows)
{
	windows.clear();

	std::vector<std::string> w;
	GetWindowList(w);
	for (auto window : w) {
		windows << QString::fromStdString(window);
	}
}

void GetCurrentWindowTitle(std::string &title)
{
	HWND window = GetForegroundWindow();
	DWORD pid;
	DWORD thid;
	thid = GetWindowThreadProcessId(window, &pid);
	// Calling GetWindowTitle() on the OBS windows might cause a deadlock in
	// the following scenario:
	//
	// The thread using GetWindowTitle() will send a WM_GETTEXT message to
	// the main thread that created the window and wait for it to be
	// processed.
	// The main thread itself might be blocked from processing the message,
	// however, when it itself is waiting for the thread using
	// GetWindowTitle() to return.
	//
	// So instead rely on Qt to get the title of the active window.
	if (GetCurrentProcessId() == pid) {
		auto window = QApplication::activeWindow();
		if (window) {
			title = window->windowTitle().toStdString();
		} else {
			title = "OBS";
		}
		return;
	}
	GetWindowTitle(window, title);
}

static HWND getHWNDfromTitle(std::string title)
{
	HWND hwnd = NULL;
	wchar_t wTitle[512];
	os_utf8_to_wcs(title.c_str(), 0, wTitle, 512);
	hwnd = FindWindowEx(NULL, NULL, NULL, wTitle);
	return hwnd;
}

bool IsMaximized(const std::string &title)
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

static std::wstring GetControlText(HWND hwnd, IUIAutomationElement *element)
{
	VARIANT var;
	std::wstring text = L"";
	HRESULT hr = element->GetCurrentPropertyValue(UIA_NamePropertyId, &var);
	if (SUCCEEDED(hr)) {
		if (var.vt == VT_BSTR && var.bstrVal) {
			text = var.bstrVal;
		}
		VariantClear(&var);
	}

	hr = element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var);
	if (SUCCEEDED(hr)) {
		if (var.vt == VT_BSTR && var.bstrVal) {
			if (!text.empty()) {
				text += L"\n";
			}
			text += +var.bstrVal;
		}
		VariantClear(&var);
	}

	return text;
}

std::optional<std::string> GetTextInWindow(const std::string &window)
{
	HWND hwnd = getHWNDfromTitle(window);
	if (!hwnd) {
		return {};
	}

	IUIAutomation *automation = nullptr;
	auto hr = CoCreateInstance(__uuidof(CUIAutomation), nullptr,
				   CLSCTX_INPROC_SERVER,
				   __uuidof(IUIAutomation),
				   reinterpret_cast<void **>(&automation));
	if (FAILED(hr)) {
		return {};
	}

	IUIAutomationElement *rootElement = nullptr;
	hr = automation->ElementFromHandle(hwnd, &rootElement);
	if (FAILED(hr)) {
		automation->Release();
		return {};
	}

	IUIAutomationTreeWalker *walker = nullptr;
	hr = automation->get_ControlViewWalker(&walker);
	if (FAILED(hr)) {
		rootElement->Release();
		automation->Release();
		return {};
	}

	IUIAutomationElement *element = nullptr;
	std::wstring result;

	hr = walker->GetFirstChildElement(rootElement, &element);
	while (SUCCEEDED(hr) && element != nullptr) {
		auto text = GetControlText(hwnd, element);
		if (!text.empty()) {
			result += text + L"\n";
		}

		IUIAutomationElement *nextElement = nullptr;
		hr = walker->GetNextSiblingElement(element, &nextElement);
		element->Release();
		element = nextElement;
	}

	walker->Release();
	rootElement->Release();
	automation->Release();

	// Convert to std::string
	int len = os_wcs_to_utf8(result.c_str(), 0, nullptr, 0);
	std::string tmp;
	tmp.resize(len);
	os_wcs_to_utf8(result.c_str(), 0, &tmp[0], len + 1);
	return tmp;
}

bool IsFullscreen(const std::string &title)
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

static void GetForegroundProcessName(QString &proc)
{
	// only checks if the current foreground window is from the same executable,
	// may return true for any window from a program
	HWND foregroundWindow = GetForegroundWindow();
	DWORD processId = 0;
	GetWindowThreadProcessId(foregroundWindow, &processId);

	HANDLE process = OpenProcess(
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
	if (process == NULL) {
		return;
	}

	WCHAR executablePath[600];
	GetModuleFileNameEx(process, 0, executablePath, 600);
	CloseHandle(process);

	proc = QString::fromWCharArray(executablePath)
		       .split(QRegularExpression("(/|\\\\)"))
		       .back();
}

void GetForegroundProcessName(std::string &proc)
{
	QString temp;
	GetForegroundProcessName(temp);
	proc = temp.toStdString();
}

bool IsInFocus(const QString &executable)
{
	// only checks if the current foreground window is from the same executable,
	// may return true for any window from a program
	QString foregroundProc;
	GetForegroundProcessName(foregroundProc);

	// True if executable switch equals current window
	bool equals = (executable == foregroundProc);
	// True if executable switch matches current window
	bool matches = foregroundProc.contains(QRegularExpression(executable));

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

void PressKeys(const std::vector<HotkeyType> keys, int duration)
{
	const int repeatInterval = 100;

	INPUT ip;
	ip.type = INPUT_KEYBOARD;
	ip.ki.wScan = 0;
	ip.ki.time = 0;
	ip.ki.dwExtraInfo = 0;

	for (int cur = 0; cur < duration; cur += repeatInterval) {
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
		Sleep(repeatInterval);
	}

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

static int getLastInputTime()
{
	LASTINPUTINFO lastInputInfo;
	lastInputInfo.cbSize = sizeof(LASTINPUTINFO);
	if (GetLastInputInfo(&lastInputInfo)) {
		return lastInputInfo.dwTime;
	}
	return 0;
}

static int getTime()
{
	return GetTickCount();
}

int SecondsSinceLastInput()
{
	return (getTime() - getLastInputTime()) / 1000;
}

static void handleRawMouseInput(LPARAM lParam)
{
	UINT dwSize;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize,
			sizeof(RAWINPUTHEADER));
	LPBYTE lpb = new BYTE[dwSize];
	if (lpb == NULL) {
		return;
	}

	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize,
			    sizeof(RAWINPUTHEADER)) != dwSize)
		OutputDebugString(TEXT(
			"GetRawInputData does not return correct size !\n"));

	RAWINPUT *raw = (RAWINPUT *)lpb;
	if (raw->header.dwType == RIM_TYPEMOUSE) {
		switch (raw->data.mouse.usButtonFlags) {
		case RI_MOUSE_BUTTON_1_DOWN:
			lastMouseLeftClickTime =
				std::chrono::high_resolution_clock::now();
			break;
		case RI_MOUSE_BUTTON_3_DOWN:
			lastMouseMiddleClickTime =
				std::chrono::high_resolution_clock::now();
			break;
		case RI_MOUSE_BUTTON_2_DOWN:
			lastMouseRightClickTime =
				std::chrono::high_resolution_clock::now();
			break;
		default:
			break;
		}
	}
	delete[] lpb;
}

class RawMouseInputFilter : public QAbstractNativeEventFilter {
public:
	virtual bool nativeEventFilter(const QByteArray &eventType,
				       void *message, qintptr *) Q_DECL_OVERRIDE
	{
		if (eventType != "windows_generic_MSG") {
			return false;
		}
		MSG *msg = reinterpret_cast<MSG *>(message);

		if (msg->message != WM_INPUT) {
			return false;
		}
		handleRawMouseInput(msg->lParam);
		return false;
	}
};

static void SetupMouseEeventFilter()
{
	mouseInputFilter = new RawMouseInputFilter;
	QAbstractEventDispatcher::instance()->installNativeEventFilter(
		mouseInputFilter);

	RAWINPUTDEVICE rid;
	rid.dwFlags = RIDEV_INPUTSINK;
	rid.usUsagePage = 1;
	rid.usUsage = 2;
	rid.hwndTarget = (HWND)obs_frontend_get_main_window_handle();
	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
		blog(LOG_WARNING, "Registering for raw mouse input failed!\n"
				  "Mouse click detection not functional!");
	}
}

void PlatformInit()
{
	SetupMouseEeventFilter();

	CoInitialize(NULL);
}

void PlatformCleanup()
{
	if (mouseInputFilter) {
		delete mouseInputFilter;
		mouseInputFilter = nullptr;
	}

	CoUninitialize();
}

} // namespace advss
