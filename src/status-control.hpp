#pragma once
#include <QDockWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <obs-data.h>

class StatusControl : public QWidget {
	Q_OBJECT

public:
	StatusControl(QWidget *parent = 0, bool noLayout = false);
	QPushButton *Button() { return _button; }
	QLabel *StatusLabel() { return _status; }
	QLabel *StatusPrefixLabel() { return _statusPrefix; }

private slots:
	void ButtonClicked();
	void UpdateStatus();

private:
	void SetStopped();
	void SetStarted();

	QPushButton *_button;
	QLabel *_status;
	QLabel *_statusPrefix;
	QTimer _timer;
	QMetaObject::Connection _pulse;

	bool _setToStopped = true;
};

// Only used to enable applying "OBSDock" stylesheet
class OBSDock : public QDockWidget {
	Q_OBJECT

public:
	inline OBSDock(QWidget *parent = nullptr) : QDockWidget(parent) {}
};

class StatusDock : public OBSDock {
	Q_OBJECT

public:
	StatusDock(QWidget *parent = 0);
};

void SetupDock();
