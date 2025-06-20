#include "kwin-helpers.h"

#include "log-helper.hpp"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QTextStream>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusReply>

namespace advss {

int FocusNotifier::activePID = -1;
std::string FocusNotifier::activeTitle = {};

int FocusNotifier::getActiveWindowPID()
{
	return activePID;
}

std::string FocusNotifier::getActiveWindowTitle()
{
	return activeTitle;
}

void FocusNotifier::focusChanged(const int pid)
{
	activePID = pid;
}

void FocusNotifier::focusTitle(const QString &title)
{
	activeTitle = title.toStdString();
}

bool isKWinAvailable()
{
	const QDBusConnectionInterface *interface =
		QDBusConnection::sessionBus().interface();
	if (!interface)
		return false;

	const QStringList services =
		interface->registeredServiceNames().value();
	return services.contains("org.kde.KWin");
}

bool startKWinScript(QString &scriptObjectPath)
{
	const QString scriptPath =
		"/tmp/AdvancedSceneSwitcher/KWinFocusNotifier.js";

	const QString script =
		R"(workspace.windowActivated.connect(function(client) {
if (!client) return;
if (!client.pid) return;
if (!client.caption) return;

callDBus(
    "com.github.AdvancedSceneSwitcher",
	"/com/github/AdvancedSceneSwitcher",
	"com.github.AdvancedSceneSwitcher",
	"focusChanged",
	client.pid
);
callDBus(
    "com.github.AdvancedSceneSwitcher",
	"/com/github/AdvancedSceneSwitcher",
	"com.github.AdvancedSceneSwitcher",
	"focusTitle",
	client.caption
);
}))";

	if (const QDir dir; !dir.mkpath(QFileInfo(scriptPath).absolutePath())) {
		blog(LOG_ERROR, "error creating /tmp/AdvancedSceneSwitcher");
		return false;
	}

	QFile scriptFile(scriptPath);
	if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
		blog(LOG_ERROR,
		     "error opening KWinFocusNotifier.js for writing");
		return false;
	}

	scriptFile.setPermissions(QFileDevice::ReadOwner |
				  QFileDevice::WriteOwner);
	QTextStream outputStream(&scriptFile);
	outputStream << script;
	scriptFile.close();

	const QDBusConnection bus = QDBusConnection::sessionBus();

	QDBusInterface scriptingIface("org.kde.KWin", "/Scripting",
				      "org.kde.kwin.Scripting", bus);
	if (!scriptingIface.isValid()) {
		return false;
	}

	const QDBusReply<bool> scriptRunningReply =
		scriptingIface.call("isScriptLoaded", scriptPath);
	if (scriptRunningReply.isValid() && scriptRunningReply.value()) {
		// script already registered and running, don't do it again
		// this will leave the script running since we do not have
		// a valid script id anymore, but at the very least this prevents
		// it from running multiple times
		return true;
	}

	const QDBusReply<int> scriptIdReply =
		scriptingIface.call("loadScript", scriptPath);
	if (!scriptIdReply.isValid()) {
		return false;
	}

	const int scriptId = scriptIdReply.value();
	scriptObjectPath =
		QString("/Scripting/Script%1").arg(QString::number(scriptId));

	QDBusInterface scriptRunner("org.kde.KWin", scriptObjectPath,
				    "org.kde.kwin.Script", bus);
	if (!scriptRunner.isValid()) {
		return false;
	}

	const QDBusReply<void> runReply = scriptRunner.call("run");
	return runReply.isValid();
}

bool stopKWinScript(const QString &scriptObjectPath)
{
	QDBusInterface scriptRunner("org.kde.KWin", scriptObjectPath,
				    "org.kde.kwin.Script",
				    QDBusConnection::sessionBus());
	if (!scriptRunner.isValid()) {
		return false;
	}
	const QDBusReply<void> stopReply = scriptRunner.call("stop");
	return stopReply.isValid();
}

bool registerKWinDBusListener(FocusNotifier *notifier)
{
	static const QString serviceName = "com.github.AdvancedSceneSwitcher";
	static const QString objectPath = "/com/github/AdvancedSceneSwitcher";
	auto bus = QDBusConnection::sessionBus();

	if (bus.objectRegisteredAt(objectPath)) {
		// already registered?
		return true;
	}

	if (!bus.registerService(serviceName)) {
		return false;
	}

	if (!bus.registerObject(objectPath, notifier,
				QDBusConnection::ExportAllSlots)) {
		return false;
	}
	return true;
}
} // namespace advss
