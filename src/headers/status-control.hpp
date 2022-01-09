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

class StatusDock : public QDockWidget {
	Q_OBJECT

public:
	StatusDock(QWidget *parent = 0);
};

extern StatusDock *dock;

void saveDock(obs_data_t *obj);
void loadDock(obs_data_t *obj);
