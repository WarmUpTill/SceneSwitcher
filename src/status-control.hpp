#pragma once
#include "obs-dock.hpp"

#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QLayout>
#include <obs-data.h>

namespace advss {

class StatusControl : public QWidget {
	Q_OBJECT

public:
	StatusControl(QWidget *parent = 0, bool noLayout = false);
	QPushButton *Button() { return _button; }
	QHBoxLayout *ButtonLayout() { return _buttonLayout; }
	QLabel *StatusLabel() { return _status; }
	QLabel *StatusPrefixLabel() { return _statusPrefix; }

private slots:
	void ButtonClicked();
	void UpdateStatus();

private:
	void SetStopped();
	void SetStarted();

	QPushButton *_button;
	QHBoxLayout *_buttonLayout;
	QLabel *_status;
	QLabel *_statusPrefix;
	QTimer _timer;
	QMetaObject::Connection _pulse;

	bool _setToStopped = true;
};

class StatusDock : public OBSDock {
	Q_OBJECT

public:
	StatusDock(QWidget *parent = 0);
};

void SetupDock();

} // namespace advss
