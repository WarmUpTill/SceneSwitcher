#include "headers/status-dock.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

#include <obs-module.h>
#include <QMainWindow>
#include <QLayout>

StatusDock *dock = nullptr;

StatusDock::StatusDock(QWidget *parent)
	: QDockWidget(obs_module_text("AdvSceneSwitcher.windowTitle"), parent)
{
	_button = new QPushButton(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	_status =
		new QLabel(obs_module_text("AdvSceneSwitcher.status.inactive"));
	QWidget::connect(_button, SIGNAL(clicked()), this,
			 SLOT(ButtonClicked()));
	QHBoxLayout *statusLayout = new QHBoxLayout;
	statusLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.generalTab.status.currentStatus")));
	statusLayout->addWidget(_status);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addLayout(statusLayout);
	layout->addWidget(_button);
	layout->addStretch();
	QWidget *tmp = new QWidget;
	tmp->setLayout(layout);
	setWidget(tmp);
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateStatus()));
	_timer.start(1000);
}

void StatusDock::ButtonClicked()
{
	if (switcher->th && switcher->th->isRunning()) {
		switcher->Stop();
		SetStopped();
	} else {
		switcher->Start();
		SetStarted();
	}
}

void StatusDock::UpdateStatus()
{
	if (switcher->th && switcher->th->isRunning()) {
		SetStopped();
	} else {
		SetStarted();
	}
}

void StatusDock::SetStopped()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.stop"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.active"));
}

void StatusDock::SetStarted()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.inactive"));
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
