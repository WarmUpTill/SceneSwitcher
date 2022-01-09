#include "headers/status-control.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <obs-module.h>
#include <QMainWindow>
#include <QLayout>

StatusDock *dock = nullptr;

StatusControl::StatusControl(QWidget *parent, bool noLayout) : QWidget(parent)
{
	_button = new QPushButton("-", this);
	_status = new QLabel("-", this);
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
				     QColor(0, 0, 0, 0), "QLabel ");
	}
	_setToStopped = true;
}

StatusDock::StatusDock(QWidget *parent)
	: QDockWidget(obs_module_text("AdvSceneSwitcher.windowTitle"), parent)
{
	// Not sure why an extra QWidget wrapper is necessary...
	// without it the dock widget seems to be partially transparent.
	QWidget *tmp = new QWidget;
	QHBoxLayout *layout = new QHBoxLayout;
	layout->addWidget(new StatusControl(this));
	tmp->setLayout(layout);
	setWidget(tmp);
}

void saveDock(obs_data_t *obj)
{
	obs_data_set_bool(obj, "statusDockVisible", dock->isVisible());
	obs_data_set_bool(obj, "statusDockFloating", dock->isFloating());
	obs_data_set_int(obj, "statusDockPosX", dock->pos().x());
	obs_data_set_int(obj, "statusDockPosY", dock->pos().y());
	obs_data_set_int(obj, "statusDockPosWidth", dock->width());
	obs_data_set_int(obj, "statusDockPosHeight", dock->height());
}

void loadDock(obs_data_t *obj)
{
	dock->setVisible(obs_data_get_bool(obj, "statusDockVisible"));
	dock->setFloating(obs_data_get_bool(obj, "statusDockFloating"));
	QPoint pos = {
		static_cast<int>(obs_data_get_int(obj, "statusDockPosX")),
		static_cast<int>(obs_data_get_int(obj, "statusDockPosY"))};
	if (windowPosValid(pos)) {
		dock->resize(obs_data_get_int(obj, "statusDockPosWidth"),
			     obs_data_get_int(obj, "statusDockPosHeight"));
		dock->move(pos);
	}
}
