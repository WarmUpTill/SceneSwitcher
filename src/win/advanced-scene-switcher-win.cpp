#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objectarray.h>
#include <shobjidl_core.h>
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
#define MAXDESKTOPS 255

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

bool isMaximized(std::string &title)
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

bool isFullscreen(std::string &title)
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

// Virtual Desktop code
// based on https://github.com/Eun/MoveToDesktop/
//
// TODO:
// - clean up
// - call FreeCom() on exit

struct IApplicationView : public IUnknown {
public:
};

MIDL_INTERFACE("FF72FFDD-BE7E-43FC-9C03-AD81681E88E4")
IVirtualDesktop : public IUnknown
{
public:
	virtual HRESULT STDMETHODCALLTYPE IsViewVisible(
		IApplicationView * pView, int *pfVisible) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetID(GUID * pGuid) = 0;
};

class IVirtualDesktopManagerInternal : public IUnknown {
public:
	virtual HRESULT STDMETHODCALLTYPE GetCount(UINT *pCount) = 0;

	virtual HRESULT STDMETHODCALLTYPE MoveViewToDesktop(
		IApplicationView *pView, IVirtualDesktop *pDesktop) = 0;

	// 10240
	virtual HRESULT STDMETHODCALLTYPE CanViewMoveDesktops(
		IApplicationView *pView, int *pfCanViewMoveDesktops) = 0;

	virtual HRESULT STDMETHODCALLTYPE
	GetCurrentDesktop(IVirtualDesktop **desktop) = 0;

	virtual HRESULT STDMETHODCALLTYPE
	GetDesktops(IObjectArray **ppDesktops) = 0;

	virtual HRESULT STDMETHODCALLTYPE
	SwitchDesktop(IVirtualDesktop *pDesktop) = 0;

	virtual HRESULT STDMETHODCALLTYPE
	CreateDesktopW(IVirtualDesktop **ppNewDesktop) = 0;

	// pFallbackDesktop - ??????? ???? ?? ??????? ????? ???????? ??????? ????? ???????? ??????????
	virtual HRESULT STDMETHODCALLTYPE
	RemoveDesktop(IVirtualDesktop *pRemove,
		      IVirtualDesktop *pFallbackDesktop) = 0;

	// 10240
	virtual HRESULT STDMETHODCALLTYPE
	FindDesktop(GUID *desktopId, IVirtualDesktop **ppDesktop) = 0;
};

IServiceProvider *pServiceProvider = nullptr;
IVirtualDesktopManager *pDesktopManager = nullptr;
IVirtualDesktopManagerInternal *pDesktopManagerInternal = nullptr;

const CLSID CLSID_ImmersiveShell = {0xC2F03A33, 0x21F5, 0x47FA, 0xB4,
				    0xBB,       0x15,   0x63,   0x62,
				    0xA2,       0xF2,   0x39};

const CLSID CLSID_VirtualDesktopAPI_Unknown = {0xC5E0CDCA, 0x7B6E, 0x41B2, 0x9F,
					       0xC4,       0xD9,   0x39,   0x75,
					       0xCC,       0x46,   0x7B};

const IID IID_IVirtualDesktopManagerInternal = {0xEF9F1A6C, 0xD3CC, 0x4358,
						0xB7,       0x12,   0xF8,
						0x4B,       0x63,   0x5B,
						0xEB,       0xE7};

const IID UUID_IVirtualDesktopManagerInternal_10130{0xEF9F1A6C, 0xD3CC, 0x4358,
						    0xB7,       0x12,   0xF8,
						    0x4B,       0x63,   0x5B,
						    0xEB,       0xE7};
const IID UUID_IVirtualDesktopManagerInternal_10240{0xAF8DA486, 0x95BB, 0x4460,
						    0xB3,       0xB7,   0x6E,
						    0x7A,       0x6B,   0x29,
						    0x62,       0xB5};
const IID UUID_IVirtualDesktopManagerInternal_14393{0xf31574d6, 0xb682, 0x4cdc,
						    0xbd,       0x56,   0x18,
						    0x27,       0x86,   0x0a,
						    0xbe,       0xc6};

enum EComStatus {
	COMSTATUS_UNINITIALIZED,
	COMSTATUS_INITIALIZED,
	COMSTATUS_ERROR,
};

int ComStatus = COMSTATUS_UNINITIALIZED;
#define COMMAND_TIMEOUT 500 // Blocks Command below this timeout
bool bAddedMenu = false;
bool bReadIni = false;
bool bSwitchDesktopAfterMove = false;
bool bCreateNewDesktopOnMove = false;
bool bDeleteEmptyDesktops = true;
ULONGLONG nLastCommand = 0;

UINT16 GetWinBuildNumber()
{
	UINT16 buildNumbers[] = {10130, 10240, 14393};
	OSVERSIONINFOEXW osvi = {sizeof(osvi), 0, 0, 0, 0, {0}, 0, 0};
	ULONGLONG mask = ::VerSetConditionMask(0, VER_BUILDNUMBER, VER_EQUAL);

	for (size_t i = 0; i < sizeof(buildNumbers) / sizeof(buildNumbers[0]);
	     i++) {
		osvi.dwBuildNumber = buildNumbers[i];
		if (VerifyVersionInfoW(&osvi, VER_BUILDNUMBER, mask) != FALSE) {
			return buildNumbers[i];
		}
	}

	return 0;
}

BOOL InitCom()
{
	if (ComStatus == COMSTATUS_INITIALIZED) {
		return true;
	} else if (ComStatus == COMSTATUS_ERROR) {
		return false;
	}

	ComStatus = COMSTATUS_ERROR;
	HRESULT hr = ::CoInitialize(NULL);
	if (FAILED(hr)) {
		return FALSE;
	}

	hr = ::CoCreateInstance(CLSID_ImmersiveShell, NULL, CLSCTX_LOCAL_SERVER,
				__uuidof(IServiceProvider),
				(PVOID *)&pServiceProvider);
	if (FAILED(hr)) {
		return FALSE;
	}

	hr = pServiceProvider->QueryService(__uuidof(IVirtualDesktopManager),
					    &pDesktopManager);
	if (FAILED(hr)) {
		pServiceProvider->Release();
		pServiceProvider = nullptr;
		return FALSE;
	}

	UINT16 buildNumber = GetWinBuildNumber();

	switch (buildNumber) {
	case 10130:
		hr = pServiceProvider->QueryService(
			CLSID_VirtualDesktopAPI_Unknown,
			UUID_IVirtualDesktopManagerInternal_10130,
			(void **)&pDesktopManagerInternal);
		break;
	case 10240:
		hr = pServiceProvider->QueryService(
			CLSID_VirtualDesktopAPI_Unknown,
			UUID_IVirtualDesktopManagerInternal_10240,
			(void **)&pDesktopManagerInternal);
		break;
	case 14393:
	default:
		hr = pServiceProvider->QueryService(
			CLSID_VirtualDesktopAPI_Unknown,
			UUID_IVirtualDesktopManagerInternal_14393,
			(void **)&pDesktopManagerInternal);
		break;
	}
	if (FAILED(hr)) {
		pDesktopManager->Release();
		pDesktopManager = nullptr;
		pServiceProvider->Release();
		pServiceProvider = nullptr;
		return FALSE;
	}

	ComStatus = COMSTATUS_INITIALIZED;
	return TRUE;
}

VOID FreeCom()
{
	if (ComStatus == COMSTATUS_INITIALIZED) {
		pDesktopManager->Release();
		pDesktopManagerInternal->Release();
		pServiceProvider->Release();
		ComStatus = COMSTATUS_UNINITIALIZED;
	}
}

bool GetCurrentVirtualDesktop(long &desktop)
{
	UINT count = 0;
	IObjectArray *pObjectArray = nullptr;
	IVirtualDesktop *pCurrentDesktop = nullptr;

	if (!InitCom()) {
		return false;
	}

	HRESULT hr = pDesktopManagerInternal->GetDesktops(&pObjectArray);
	if (FAILED(hr)) {
		return false;
	}

	hr = pObjectArray->GetCount(&count);
	if (FAILED(hr)) {
		pObjectArray->Release();
		return false;
	}

	hr = pDesktopManagerInternal->GetCurrentDesktop(&pCurrentDesktop);
	if (FAILED(hr)) {
		pObjectArray->Release();
		return false;
	}
	int index = -1;
	for (UINT i = 0; i < count && i < MAXDESKTOPS && index == -1; ++i) {
		IVirtualDesktop *pDesktop = nullptr;

		if (FAILED(pObjectArray->GetAt(i, __uuidof(IVirtualDesktop),
					       (void **)&pDesktop)))
			continue;
		if (pDesktop == pCurrentDesktop) {
			index = i;
		}
		pDesktop->Release();
	}

	pObjectArray->Release();

	if (pCurrentDesktop != nullptr) {
		pCurrentDesktop->Release();
	}
	desktop = index;
	return true;
}

bool GetVirtualDesktopCount(long &ndesktops)
{
	if (!InitCom()) {
		return false;
	}
	IObjectArray *pObjectArray = nullptr;
	HRESULT hr = pDesktopManagerInternal->GetDesktops(&pObjectArray);
	if (FAILED(hr)) {
		return false;
	}

	UINT count;
	hr = pObjectArray->GetCount(&count);
	if (FAILED(hr)) {
		pObjectArray->Release();
		return false;
	}

	pObjectArray->Release();
	ndesktops = count;

	return true;
}
