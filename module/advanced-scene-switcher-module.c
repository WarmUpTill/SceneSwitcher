#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("advanced-scene-switcher", "en-US")

typedef const char *(*translateFunc)(const char *);

void InitSceneSwitcher(obs_module_t *, translateFunc);
void RunPluginPostLoadSteps();
void FreeSceneSwitcher();

void obs_module_post_load(void)
{
	RunPluginPostLoadSteps();
}

bool obs_module_load(void)
{
	obs_frontend_push_ui_translation(obs_module_get_string);
	InitSceneSwitcher(obs_current_module(), obs_module_text);
	return true;
}

void obs_module_unload(void)
{
	FreeSceneSwitcher();
}
