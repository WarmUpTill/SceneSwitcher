#include "obs-script-helpers.hpp"
#include "log-helper.hpp"

#include <QFileInfo>
#include <QLibrary>

namespace advss {

static const char *libName =
#if defined(WIN32)
	"obs-scripting.dll";
#elif __APPLE__
	"obs-scripting.dylib";
#else
	"obs-scripting.so";
#endif

typedef obs_script_t *(*obs_script_create_t)(const char *, obs_data_t *);
typedef void (*obs_script_destroy_t)(obs_script_t *);

obs_script_create_t obs_script_create = nullptr;
obs_script_destroy_t obs_script_destroy = nullptr;

static bool setup()
{
	QLibrary scriptingLib(libName);

	obs_script_create =
		(obs_script_create_t)scriptingLib.resolve("obs_script_create");
	if (!obs_script_create) {
		blog(LOG_WARNING,
		     "could not resolve obs_script_create symbol!");
	}

	obs_script_destroy = (obs_script_destroy_t)scriptingLib.resolve(
		"obs_script_destroy");
	if (!obs_script_destroy) {
		blog(LOG_WARNING,
		     "could not resolve obs_script_destroy symbol!");
	}

	return true;
}
static bool setupDone = setup();

obs_script_t *CreateOBSScript(const char *path, obs_data_t *settings)
{
	if (!obs_script_create) {
		return nullptr;
	}
	return obs_script_create(path, settings);
}

void DestroyOBSScript(obs_script_t *script)
{
	if (!obs_script_destroy) {
		return;
	}
	obs_script_destroy(script);
}

std::string GetLUACompatiblePath(const std::string &path)
{
	// Can't use settingsFile here as LUA will complain if Windows style
	// paths (C:\some\path) are used.
	// QFileInfo::absoluteFilePath will convert those paths so LUA won't
	// complain. (C:/some/path)
	const QFileInfo fileInfo(QString::fromStdString(path));
	return fileInfo.absoluteFilePath().toStdString();
}

} // namespace advss
