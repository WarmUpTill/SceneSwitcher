#include <obs-module.h>

OBS_DECLARE_MODULE()

void InitSceneSwitcher();
void FreeSceneSwitcher();

bool obs_module_load(void)
{
	InitSceneSwitcher();
	return true;
}

void obs_module_unload(void)
{
	FreeSceneSwitcher();
}
