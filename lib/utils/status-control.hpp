#pragma once
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
	void SetStatusStyleSheet(bool stopped) const;

	QPushButton *_button;
	QHBoxLayout *_buttonLayout;
	QLabel *_status;
	QLabel *_statusPrefix;
	QTimer _timer;
	QObject *_pulse = nullptr;

	bool _setToStopped = true;
};

class StatusDockWidget : public QFrame {
	Q_OBJECT

public:
	StatusDockWidget(QWidget *parent = 0);
};

void SetupDock();

} // namespace advss
