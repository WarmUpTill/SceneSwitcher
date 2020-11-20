#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("advanced-scene-switcher", "en-US")

void InitSceneSwitcher();
void FreeSceneSwitcher();

bool obs_module_load(void)
{
	obs_frontend_push_ui_translation(obs_module_get_string);
	InitSceneSwitcher();
	return true;
}

void obs_module_unload(void)
{
	FreeSceneSwitcher();
}
