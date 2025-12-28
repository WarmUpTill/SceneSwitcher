#include "crash-handler.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>

#include <thread>

namespace advss {

static constexpr std::string_view sentinel = ".running";

#ifndef NDEBUG
static constexpr bool handleUncleanShutdown = false;
#else
static constexpr bool handleUncleanShutdown = true;
#endif

static bool wasCleanShutdown = false;

static void setup();
static bool setupDone = []() {
	AddPluginInitStep(setup);
	return true;
}();

static void handleShutdown(enum obs_frontend_event event, void *)
{
	if (event != OBS_FRONTEND_EVENT_EXIT) {
		return;
	}

	char *sentinelFile = obs_module_config_path(sentinel.data());
	if (!sentinelFile) {
		return;
	}

	QFile file(sentinelFile);
	if (!file.exists()) {
		bfree(sentinelFile);
		return;
	}

	if (!file.remove()) {
		blog(LOG_WARNING, "failed to remove sentinel file");
	}

	bfree(sentinelFile);
}

static void setup()
{
	char *sentinelFile = obs_module_config_path(sentinel.data());
	if (!sentinelFile) {
		return;
	}

	QString dirPath = QFileInfo(sentinelFile).absolutePath();
	QDir dir(dirPath);
	if (!dir.exists()) {
		if (!dir.mkpath(dirPath)) {
			blog(LOG_WARNING,
			     "failed to create directory for sentinel file");
			bfree(sentinelFile);
			return;
		}
	}

	QFile file(sentinelFile);

	wasCleanShutdown = file.exists();

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		blog(LOG_WARNING, "failed to create sentinel file");
		bfree(sentinelFile);
		return;
	}

	file.write("running");
	file.close();
	bfree(sentinelFile);

	obs_frontend_add_event_callback(handleShutdown, nullptr);
	return;
}

static bool wasUncleanShutdown()
{
	static bool alreadyHandled = false;

	if (!handleUncleanShutdown || !wasCleanShutdown || alreadyHandled) {
		alreadyHandled = true;
		return false;
	}

	alreadyHandled = true;
	blog(LOG_WARNING, "unclean shutdown detected");
	return true;
}

static bool askForStartupSkip()
{
	return DisplayMessage(obs_module_text("AdvSceneSwitcher.crashDetected"),
			      true, false);
}

bool ShouldSkipPluginStartOnUncleanShutdown()
{
	if (!wasUncleanShutdown()) {
		return false;
	}

	std::thread t([]() {
		obs_queue_task(
			OBS_TASK_UI,
			[](void *) {
				const bool skipStart = askForStartupSkip();
				if (!skipStart) {
					StartPlugin();
				}
			},
			nullptr, false);
	});
	t.detach();

	return true;
}

} // namespace advss
