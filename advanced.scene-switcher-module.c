#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("frontend-tools", "en-US")

void InitSceneSwitcher();
void FreeSceneSwitcher();
void stopSwitcher();


//void startStopHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
//	UNUSED_PARAMETER(data);
//	UNUSED_PARAMETER(hotkey);
//	if (pressed)
//	{
//		//startStopSwitcher();
//	}
//	//obs_data_array_release(pauseHotkeyData);
//	//pauseHotkeyData = obs_hotkey_save(id);
//}

bool obs_module_load(void)
{
	InitSceneSwitcher();
	//obs_hotkey_register_frontend("startStopSwitcherHotkey", "Toggle Start/Stop for the Advanced Scene Switcher", startStopHotkeyFunc, NULL);
	return true;
}

void obs_module_unload(void)
{
	FreeSceneSwitcher();
}
