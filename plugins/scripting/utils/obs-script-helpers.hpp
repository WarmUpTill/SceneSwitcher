#pragma once
#include <obs-data.h>

#include <string>

// Based on obs-scripting.h

struct obs_script;
typedef struct obs_script obs_script_t;

enum obs_script_lang {
	OBS_SCRIPT_LANG_UNKNOWN,
	OBS_SCRIPT_LANG_LUA,
	OBS_SCRIPT_LANG_PYTHON
};

namespace advss {

obs_script_t *CreateOBSScript(const char *path, obs_data_t *settings);
void DestroyOBSScript(obs_script_t *script);
std::string GetLUACompatiblePath(const std::string &path);

} // namespace advss
