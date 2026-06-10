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

std::mutex FocusNotifier::_mutex;
int FocusNotifier::activePID = -1;
std::string FocusNotifier::activeTitle = {};
std::map<std::string, std::pair<int, std::string>> FocusNotifier::_windows;

int FocusNotifier::getActiveWindowPID()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return activePID;
}

std::string FocusNotifier::getActiveWindowTitle()
{
	std::lock_guard<std::mutex> lock(_mutex);
	return activeTitle;
}

std::vector<std::pair<int, std::string>> FocusNotifier::getWindowList()
{
	std::lock_guard<std::mutex> lock(_mutex);
	std::vector<std::pair<int, std::string>> result;
	result.reserve(_windows.size());
	for (const auto &entry : _windows) {
		result.emplace_back(entry.second);
	}
	return result;
}

void FocusNotifier::focusChanged(const int pid)
{
	std::lock_guard<std::mutex> lock(_mutex);
	activePID = pid;
}

void FocusNotifier::focusTitle(const QString &title)
{
	std::lock_guard<std::mutex> lock(_mutex);
	activeTitle = title.toStdString();
}

void FocusNotifier::focusUpdate(const int pid, const QString &title)
{
	std::lock_guard<std::mutex> lock(_mutex);
	activePID = pid;
	activeTitle = title.toStdString();
}

void FocusNotifier::windowAdded(const QString &id, const int pid,
				const QString &title)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_windows[id.toStdString()] = {pid, title.toStdString()};
}

void FocusNotifier::windowRemoved(const QString &id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	_windows.erase(id.toStdString());
}

void FocusNotifier::windowTitleChanged(const QString &id, const QString &title)
{
	std::lock_guard<std::mutex> lock(_mutex);
	auto it = _windows.find(id.toStdString());
	if (it != _windows.end()) {
		it->second.second = title.toStdString();
	}
}

bool isKWinAvailable()
{
	const QDBusConnectionInterface *interface =
		QDBusConnection::sessionBus().interface();
	if (!interface) {
		return false;
	}

	const QStringList services =
		interface->registeredServiceNames().value();
	return services.contains("org.kde.KWin");
}

bool startKWinScript(QString &scriptObjectPath)
{
	const QString scriptPath =
		"/tmp/AdvancedSceneSwitcher/KWinFocusNotifier.js";

	const QString script =
		R"(var adss = "com.github.AdvancedSceneSwitcher";
var adssPath = "/com/github/AdvancedSceneSwitcher";

function trackWindow(window) {
    var id = window.internalId.toString();
    callDBus(adss, adssPath, adss, "windowAdded", id, window.pid, window.caption);
    window.captionChanged.connect(function() {
        callDBus(adss, adssPath, adss, "windowTitleChanged", id, window.caption);
    });
}

var windows = workspace.windowList();
for (var i = 0; i < windows.length; i++) {
    trackWindow(windows[i]);
}

workspace.windowAdded.connect(trackWindow);

workspace.windowRemoved.connect(function(window) {
    callDBus(adss, adssPath, adss, "windowRemoved", window.internalId.toString());
});

workspace.windowActivated.connect(function(client) {
    callDBus(adss, adssPath, adss, "focusUpdate",
        client ? client.pid : 0,
        client ? client.caption : "");
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
