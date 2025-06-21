#pragma once

#include <QObject>
#include <QString>
#include <string>

namespace advss {

class FocusNotifier final : public QObject {
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "com.github.AdvancedSceneSwitcher")

	static int activePID;
	static std::string activeTitle;

public:
	using QObject::QObject;

	static int getActiveWindowPID();
	static std::string getActiveWindowTitle();

public slots:
	void focusChanged(const int pid);
	void focusTitle(const QString &title);
};

bool isKWinAvailable();
bool startKWinScript(QString &scriptObjectPath);
bool stopKWinScript(const QString &scriptObjectPath);
bool registerKWinDBusListener(FocusNotifier *notifier);
void printDBusError();

} // namespace advss
