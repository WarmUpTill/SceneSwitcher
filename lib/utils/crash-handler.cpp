#include "crash-handler.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QMainWindow>
#include <QTimer>

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

static void askForStartupSkip()
{
	bool skipStart = DisplayMessage(
		obs_module_text("AdvSceneSwitcher.crashDetected"), true, false);
	if (!skipStart) {
		StartPlugin();
	}
}

bool ShouldSkipPluginStartOnUncleanShutdown()
{
	if (!wasUncleanShutdown()) {
		return false;
	}

	// This function is called while the plugin settings are being loaded.
	// Blocking at this stage can cause issues such as OBS failing to start
	// or crashing.
	// Therefore, we ask the user whether they want to start the plugin
	// asynchronously.
	//
	// On macOS, an additional QTimer::singleShot wrapper is required for
	// this to work correctly.
	static const auto showDialogWrapper = [](void *) {
#ifdef __APPLE__
		QTimer::singleShot(0,
				   static_cast<QMainWindow *>(
					   obs_frontend_get_main_window()),
				   []() {
#endif
					   askForStartupSkip();
#ifdef __APPLE__
				   });
#endif
	};

	std::thread t([]() {
		obs_queue_task(OBS_TASK_UI, showDialogWrapper, nullptr, false);
	});
	t.detach();

	return true;
}

} // namespace advss
