#include <obs-module.h>
#include <obs-frontend-api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define ADVSS_SETENV(k, v) _putenv_s(k, v)
#define ADVSS_UNSETENV(k) _putenv_s(k, "")
#define ADVSS_GETPID() _getpid()
#else
#include <unistd.h>
#define ADVSS_SETENV(k, v) setenv(k, v, 1)
#define ADVSS_UNSETENV(k) unsetenv(k)
#define ADVSS_GETPID() getpid()
#endif

#define ADVSS_INSTANCE_ENV "ADVSS_INSTANCE_LOADED"

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
	char pid[32];
	snprintf(pid, sizeof(pid), "%d", (int)ADVSS_GETPID());

	const char *loadedPid = getenv(ADVSS_INSTANCE_ENV);
	if (loadedPid && strcmp(loadedPid, pid) == 0) {
		blog(LOG_WARNING,
		     "[adv-ss] Another instance is already loaded - skipping.");
		return false;
	}
	ADVSS_SETENV(ADVSS_INSTANCE_ENV, pid);
	obs_frontend_push_ui_translation(obs_module_get_string);
	InitSceneSwitcher(obs_current_module(), obs_module_text);
	return true;
}

void obs_module_unload(void)
{
	FreeSceneSwitcher();
	ADVSS_UNSETENV(ADVSS_INSTANCE_ENV);
}
