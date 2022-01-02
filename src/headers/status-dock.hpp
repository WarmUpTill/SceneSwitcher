#pragma once
#include <QDockWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <obs-data.h>

class StatusDock : public QDockWidget {
	Q_OBJECT

public:
	StatusDock(QWidget *parent = 0);

private slots:
	void ButtonClicked();
	void UpdateStatus();

private:
	void SetStopped();
	void SetStarted();

	QPushButton *_button;
	QLabel *_status;
	QTimer _timer;
};

extern StatusDock *dock;

void saveDock(obs_data_t *obj);
void loadDock(obs_data_t *obj);
