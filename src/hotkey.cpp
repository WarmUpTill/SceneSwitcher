#include "headers/hotkey.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include <obs-module.h>
#include <fstream>
#include <regex>

void startHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (!(switcher->th && switcher->th->isRunning())) {
			switcher->Start();
		}
	}
}

void stopHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		}
	}
}

void startStopToggleHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
			       bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		} else {
			switcher->Start();
		}
	}
}

void upMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
			      bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "UpMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void downMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "DownMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void removeMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				  bool pressed)
{
	if (pressed && switcher->settingsWindowOpened &&
	    AdvSceneSwitcher::window) {
		QMetaObject::invokeMethod(AdvSceneSwitcher::window,
					  "DeleteMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

void registerHotkeys()
{
	switcher->startHotkey = obs_hotkey_register_frontend(
		"startSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.startSwitcherHotkey"),
		startHotkeyFunc, NULL);
	switcher->stopHotkey = obs_hotkey_register_frontend(
		"stopSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.stopSwitcherHotkey"),
		stopHotkeyFunc, NULL);
	switcher->toggleHotkey = obs_hotkey_register_frontend(
		"startStopToggleSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.startStopToggleSwitcherHotkey"),
		startStopToggleHotkeyFunc, NULL);
	switcher->upMacroSegment = obs_hotkey_register_frontend(
		"upMacroSegmentSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.upMacroSegmentHotkey"),
		upMacroSegmentHotkeyFunc, NULL);
	switcher->downMacroSegment = obs_hotkey_register_frontend(
		"downMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.downMacroSegmentHotkey"),
		downMacroSegmentHotkeyFunc, NULL);
	switcher->removeMacroSegment = obs_hotkey_register_frontend(
		"removeMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.removeMacroSegmentHotkey"),
		removeMacroSegmentHotkeyFunc, NULL);

	switcher->hotkeysRegistered = true;
}

void saveHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_hotkey_save(id);
	obs_data_set_array(obj, name, a);
	obs_data_array_release(a);
}

void SwitcherData::saveHotkeys(obs_data_t *obj)
{
	saveHotkey(obj, startHotkey, "startHotkey");
	saveHotkey(obj, stopHotkey, "stopHotkey");
	saveHotkey(obj, toggleHotkey, "toggleHotkey");
	saveHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	saveHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	saveHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}

void loadHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_data_get_array(obj, name);
	obs_hotkey_load(id, a);
	obs_data_array_release(a);
}

void SwitcherData::loadHotkeys(obs_data_t *obj)
{
	if (!hotkeysRegistered) {
		registerHotkeys();
	}
	loadHotkey(obj, startHotkey, "startHotkey");
	loadHotkey(obj, stopHotkey, "stopHotkey");
	loadHotkey(obj, toggleHotkey, "toggleHotkey");
	loadHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	loadHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	loadHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}
