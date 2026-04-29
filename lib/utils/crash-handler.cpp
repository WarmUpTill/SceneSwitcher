#include "crash-handler.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMainWindow>
#include <QVBoxLayout>

namespace advss {

static constexpr std::string_view sentinel = ".running";

#ifndef NDEBUG
static constexpr bool handleUncleanShutdown = false;
#else
static constexpr bool handleUncleanShutdown = true;
#endif

static bool wasCleanShutdown = false;
static bool suppressCrashDialog = false;

static char *sentinelFile = nullptr;

bool GetSuppressCrashDialog()
{
	return suppressCrashDialog;
}

void SetSuppressCrashDialog(bool suppress)
{
	suppressCrashDialog = suppress;
}

static void setup();
static bool setupDone = []() {
	AddPluginInitStep(setup);
	AddSaveStep([](obs_data_t *obj) {
		obs_data_set_bool(obj, "suppressCrashDialog",
				  suppressCrashDialog);
	});
	AddLoadStep([](obs_data_t *obj) {
		suppressCrashDialog =
			obs_data_get_bool(obj, "suppressCrashDialog");
	});
	return true;
}();

static void handleShutdown(enum obs_frontend_event event, void *)
{
	if (event != OBS_FRONTEND_EVENT_EXIT) {
		return;
	}

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
	// Freed in handleShutdown()
	sentinelFile = obs_module_config_path(sentinel.data());
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
	auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto dialog = new QDialog(mainWindow);
	dialog->setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	dialog->setWindowFlags(dialog->windowFlags() &
			       ~Qt::WindowContextHelpButtonHint);

	auto layout = new QVBoxLayout(dialog);
	auto label = new QLabel(
		obs_module_text("AdvSceneSwitcher.crashDetected"), dialog);
	label->setWordWrap(true);
	layout->addWidget(label);

	auto checkbox = new QCheckBox(
		obs_module_text(
			"AdvSceneSwitcher.crashDetected.suppressCheckbox"),
		dialog);
	layout->addWidget(checkbox);

	auto buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Yes | QDialogButtonBox::No, dialog);
	QObject::connect(buttonbox, &QDialogButtonBox::accepted, dialog,
			 &QDialog::accept);
	QObject::connect(buttonbox, &QDialogButtonBox::rejected, dialog,
			 &QDialog::reject);
	layout->addWidget(buttonbox);

	dialog->setLayout(layout);

	bool skipStart = dialog->exec() == QDialog::Accepted;
	suppressCrashDialog = checkbox->isChecked();
	dialog->deleteLater();

	if (!skipStart) {
		StartPlugin();
	}
}

bool ShouldSkipPluginStartOnUncleanShutdown()
{
	if (!wasUncleanShutdown()) {
		return false;
	}

	if (suppressCrashDialog) {
		return false;
	}

	// This function is called while the plugin settings are being loaded.
	// Blocking at this stage can cause issues such as OBS failing to start
	// or crashing.
	// Therefore, we ask the user whether they want to start the plugin
	// asynchronously.
	static const auto showDialogWrapper = [](void *) {
		askForStartupSkip();
	};

	AddFinishedLoadingStep([]() {
		obs_queue_task(OBS_TASK_UI, showDialogWrapper, nullptr, false);
	});

	return true;
}

} // namespace advss
