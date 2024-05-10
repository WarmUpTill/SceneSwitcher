#include "hotkey-helpers.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>
#include <QAbstractEventDispatcher>
#include <QAbstractNativeEventFilter>
#include <unordered_map>
#include <util/platform.h>
#include <windows.h>

namespace advss {

static bool canSimulateKeyPresses = true;

bool CanSimulateKeyPresses()
{
	return canSimulateKeyPresses;
}

static const std::unordered_map<HotkeyType, long> keyTable = {
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
		for (const auto &key : keys) {
			auto it = keyTable.find(key);
			if (it == keyTable.end()) {
				continue;
			}
			ip.ki.wVk = (WORD)it->second;
			SendInput(1, &ip, sizeof(INPUT));
		}
		// When instantly releasing the key presses OBS might miss them
		Sleep(repeatInterval);
	}

	// Release keys
	ip.ki.dwFlags = KEYEVENTF_KEYUP;
	for (const auto &key : keys) {
		auto it = keyTable.find(key);
		if (it == keyTable.end()) {
			continue;
		}
		ip.ki.wVk = (WORD)it->second;
		SendInput(1, &ip, sizeof(INPUT));
	}
}

static bool windowIsValid(HWND window)
{
	if (!IsWindowVisible(window)) {
		return false;
	}

	// Only accept for top level windows and not controls within them
	if (window != GetAncestor(window, GA_ROOT)) {
		return false;
	}

	return true;
}

static std::vector<HWND> getHWNDfromTitle(const std::string &title)
{
	HWND hwnd = nullptr;
	HWND hPrevWnd = nullptr;
	std::vector<HWND> hwnds;
	wchar_t wTitle[512];
	os_utf8_to_wcs(title.c_str(), 0, wTitle, 512);
	while ((hwnd = FindWindowEx(NULL, hPrevWnd, NULL, wTitle)) != nullptr) {
		if (windowIsValid(hwnd)) {
			hwnds.emplace_back(hwnd);
		}
		hPrevWnd = hwnd;
	}
	return hwnds;
}

std::string GetWindowClassByWindowTitle(const std::string &window)
{
	auto hwnds = getHWNDfromTitle(window);
	if (hwnds.empty()) {
		return "";
	}
	auto hwnd = hwnds.at(0);
	std::wstring wClass;
	wClass.resize(1024);
	if (!GetClassNameW(hwnd, &wClass[0], wClass.capacity())) {
		return "";
	}

	size_t len = os_wcs_to_utf8(wClass.c_str(), 0, nullptr, 0);
	std::string className;
	className.resize(len);
	os_wcs_to_utf8(wClass.c_str(), 0, &className[0], len + 1);
	return className;
}

void SetFocusWindow(const std::string &title)
{
	auto hwnds = getHWNDfromTitle(title);
	if (hwnds.empty()) {
		return;
	}

	for (const auto &handle : hwnds) {
		const bool isMinimized = IsIconic(handle);
		ShowWindow(handle, isMinimized ? SW_RESTORE : SW_SHOW);
		SetForegroundWindow(handle);
		SetFocus(handle);
		SetActiveWindow(handle);
	}
}

void CloseWindow(const std::string &title)
{
	auto hwnds = getHWNDfromTitle(title);
	if (hwnds.empty()) {
		return;
	}

	for (const auto &handle : hwnds) {
		SendMessage(handle, WM_CLOSE, 0, 0);
	}
}

void MaximizeWindow(const std::string &title)
{
	auto hwnds = getHWNDfromTitle(title);
	if (hwnds.empty()) {
		return;
	}

	for (const auto &handle : hwnds) {
		ShowWindow(handle, SW_MAXIMIZE);
	}
}

void MinimizeWindow(const std::string &title)
{
	auto hwnds = getHWNDfromTitle(title);
	if (hwnds.empty()) {
		return;
	}

	for (const auto &handle : hwnds) {
		ShowWindow(handle, SW_MINIMIZE);
	}
}

class RawMouseInputFilter;
static RawMouseInputFilter *mouseInputFilter;
static std::chrono::high_resolution_clock::time_point lastMouseLeftClickTime{};
static std::chrono::high_resolution_clock::time_point lastMouseMiddleClickTime{};
static std::chrono::high_resolution_clock::time_point lastMouseRightClickTime{};

std::chrono::high_resolution_clock::time_point GetLastMouseLeftClickTime()
{
	return lastMouseLeftClickTime;
}

std::chrono::high_resolution_clock::time_point GetLastMouseMiddleClickTime()
{
	return lastMouseMiddleClickTime;
}

std::chrono::high_resolution_clock::time_point GetLastMouseRightClickTime()
{
	return lastMouseRightClickTime;
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

static void setupMouseEeventFilter()
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

static bool setup()
{
	AddPluginInitStep(setupMouseEeventFilter);
	AddPluginCleanupStep([]() {
		if (mouseInputFilter) {
			delete mouseInputFilter;
			mouseInputFilter = nullptr;
		}
	});
	return true;
}

static bool init = setup();

} // namespace advss
