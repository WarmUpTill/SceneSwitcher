#pragma once

#include <QObject>
#include <QString>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace advss {

class FocusNotifier final : public QObject {
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.github.AdvancedSceneSwitcher")

	static std::mutex _mutex;
	static int activePID;
	static std::string activeTitle;
	// Maps KWin internal window ID to {pid, title}
	static std::map<std::string, std::pair<int, std::string>> _windows;

public:
	using QObject::QObject;

	static int getActiveWindowPID();
	static std::string getActiveWindowTitle();
	static std::vector<std::pair<int, std::string>> getWindowList();

public slots:
	void focusChanged(const int pid);
	void focusTitle(const QString &title);
	void focusUpdate(const int pid, const QString &title);
	void windowAdded(const QString &id, const int pid,
			 const QString &title);
	void windowRemoved(const QString &id);
	void windowTitleChanged(const QString &id, const QString &title);
};

bool isKWinAvailable();
bool startKWinScript(QString &scriptObjectPath);
bool stopKWinScript(const QString &scriptObjectPath);
bool registerKWinDBusListener(FocusNotifier *notifier);
void printDBusError();

} // namespace advss
