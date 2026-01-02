#include "platform-funcs.hpp"

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
#include <QApplication>
#include <QWidget>
#include <mutex>

namespace advss {

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

static HWND getHWNDfromTitle(const std::string &title)
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

	DWORD pid = 0;
	DWORD thid = 0;
	thid = GetWindowThreadProcessId(hwnd, &pid);
	// Calling CoCreateInstance() on the OBS windows might cause a deadlock
	if (GetCurrentProcessId() == pid) {
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

void PlatformInit()
{
	CoInitialize(NULL);
}

void PlatformCleanup()
{
	CoUninitialize();
}

} // namespace advss
