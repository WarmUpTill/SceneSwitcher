#include <obs-module.h>
#include <fstream>
#include <regex>
#include "headers/advanced-scene-switcher.hpp"

void startHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
		     bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (!(switcher->th && switcher->th->isRunning()))
			switcher->Start();
	}
}

void stopHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey,
		    bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (switcher->th && switcher->th->isRunning())
			switcher->Stop();
	}
}

void startStopToggleHotkeyFunc(void *data, obs_hotkey_id id,
			       obs_hotkey_t *hotkey, bool pressed)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed) {
		if (switcher->th && switcher->th->isRunning())
			switcher->Stop();
		else
			switcher->Start();
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

	switcher->hotkeysRegistered = true;
}

void SwitcherData::saveHotkeys(obs_data_t *obj)
{
	obs_data_array_t *startHotkeyArrray =
		obs_hotkey_save(switcher->startHotkey);
	obs_data_set_array(obj, "startHotkey", startHotkeyArrray);
	obs_data_array_release(startHotkeyArrray);

	obs_data_array_t *stopHotkeyArrray =
		obs_hotkey_save(switcher->stopHotkey);

	obs_data_set_array(obj, "stopHotkey", stopHotkeyArrray);
	obs_data_array_release(stopHotkeyArrray);

	obs_data_array_t *toggleHotkeyArrray =
		obs_hotkey_save(switcher->toggleHotkey);
	obs_data_set_array(obj, "toggleHotkey", toggleHotkeyArrray);
	obs_data_array_release(toggleHotkeyArrray);
}

void SwitcherData::loadHotkeys(obs_data_t *obj)
{
	if (!switcher->hotkeysRegistered)
		registerHotkeys();

	obs_data_array_t *startHotkeyArrray =
		obs_data_get_array(obj, "startHotkey");
	obs_hotkey_load(switcher->startHotkey, startHotkeyArrray);
	obs_data_array_release(startHotkeyArrray);

	obs_data_array_t *stopHotkeyArrray =
		obs_data_get_array(obj, "stopHotkey");
	obs_hotkey_load(switcher->stopHotkey, stopHotkeyArrray);
	obs_data_array_release(stopHotkeyArrray);

	obs_data_array_t *toggleHotkeyArrray =
		obs_data_get_array(obj, "toggleHotkey");
	obs_hotkey_load(switcher->toggleHotkey, toggleHotkeyArrray);
	obs_data_array_release(toggleHotkeyArrray);
}
