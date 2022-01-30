#include "headers/status-control.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <obs-module.h>
#include <QMainWindow>
#include <QLayout>
#include <QAction>

StatusDock *dock = nullptr;

StatusControl::StatusControl(QWidget *parent, bool noLayout) : QWidget(parent)
{
	_button = new QPushButton("-", this);
	_status = new QLabel("-", this);
	_status->setStyleSheet("QLabel{ \
		border-style: outset; \
		border-width: 2px; \
		border-radius: 7px; \
		border-color: rgb(0,0,0,0) \
		}");
	_statusPrefix = new QLabel(
		obs_module_text(
			"AdvSceneSwitcher.generalTab.status.currentStatus"),
		this);

	QWidget::connect(_button, SIGNAL(clicked()), this,
			 SLOT(ButtonClicked()));

	if (!noLayout) {
		QHBoxLayout *statusLayout = new QHBoxLayout();
		statusLayout->addWidget(_statusPrefix);
		statusLayout->addWidget(_status);
		statusLayout->addStretch();
		QVBoxLayout *layout = new QVBoxLayout();
		layout->addLayout(statusLayout);
		layout->addWidget(_button);
		setLayout(layout);
	}

	if (switcher->stop) {
		SetStopped();
	} else {
		SetStarted();
	}
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateStatus()));
	_timer.start(1000);
}

void StatusControl::ButtonClicked()
{
	if (switcher->th && switcher->th->isRunning()) {
		switcher->Stop();
		SetStopped();
	} else {
		switcher->Start();
		SetStarted();
	}
}

void StatusControl::UpdateStatus()
{
	if (!switcher) {
		return;
	}

	if (switcher->th && switcher->th->isRunning()) {
		if (!_setToStopped) {
			return;
		}
		SetStarted();
	} else {
		if (_setToStopped) {
			return;
		}
		SetStopped();
	}
}

void StatusControl::SetStarted()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.stop"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.active"));
	_status->disconnect(_pulse);
	_setToStopped = false;
}

void StatusControl::SetStopped()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.inactive"));
	if (!switcher->disableHints) {
		_pulse = PulseWidget(_status, QColor(Qt::red),
				     QColor(0, 0, 0, 0));
	}
	_setToStopped = true;
}

StatusDock::StatusDock(QWidget *parent)
	: QDockWidget(obs_module_text("AdvSceneSwitcher.windowTitle"), parent)
{
	setFloating(true);
	// Setting a fixed object name is crucial for OBS to be able to restore
	// the docks position, if the dock is not floating
	setObjectName("Adv-ss-dock");

	// Not sure why an extra QWidget wrapper is necessary...
	// without it the dock widget seems to be partially transparent.
	QWidget *tmp = new QWidget;
	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(new StatusControl(this));
	tmp->setLayout(layout);
	setWidget(tmp);
}

void SetupDock()
{
	dock = new StatusDock(
		static_cast<QMainWindow *>(obs_frontend_get_main_window()));
	// Added for cosmetic reasons to avoid brief flash of dock window on startup
	dock->setVisible(false);
	static_cast<QAction *>(obs_frontend_add_dock(dock));
}
