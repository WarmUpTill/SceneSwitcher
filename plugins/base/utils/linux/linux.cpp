#include "hotkey-helpers.hpp"
#include "plugin-state-helpers.hpp"

#include <thread>
#include <unordered_map>

// Qt includes must happen before X11 includes
#include <QLibrary>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

namespace advss {

static QLibrary *libXtstHandle = nullptr;
typedef int (*keyPressFunc)(Display *, unsigned int, bool, unsigned long);
static keyPressFunc pressFunc = nullptr;
static bool canSimulateKeyPresses = false;

static Display *xdisplay = 0;

static Display *disp()
{
	if (!xdisplay) {
		xdisplay = XOpenDisplay(NULL);
	}

	return xdisplay;
}

static void init()
{
	libXtstHandle = new QLibrary("libXtst", nullptr);
	pressFunc = (keyPressFunc)libXtstHandle->resolve("XTestFakeKeyEvent");
	int _;
	auto display = disp();
	canSimulateKeyPresses = pressFunc && display &&
				XQueryExtension(disp(), "XTEST", &_, &_, &_);
}

static void cleanup()
{
	if (libXtstHandle) {
		delete libXtstHandle;
		libXtstHandle = nullptr;
	}
	if (!xdisplay) {
		return;
	}

	XCloseDisplay(xdisplay);
	xdisplay = nullptr;
}

static bool setup()
{
	AddPluginInitStep(init);
	AddPluginCleanupStep(cleanup);
	return true;
}

static bool setupDone = setup();

bool CanSimulateKeyPresses()
{
	return canSimulateKeyPresses;
}

static const std::unordered_map<HotkeyType, long> keyTable = {
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

void PressKeys(const std::vector<HotkeyType> &keys, int duration)
{
	if (!canSimulateKeyPresses) {
		return;
	}

	Display *display = disp();
	if (!display) {
		return;
	}

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

	std::this_thread::sleep_for(std::chrono::milliseconds(duration));

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

} // namespace advss
