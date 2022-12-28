#include "status-control.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

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

#include <QToolBar>

StatusDock::StatusDock(QWidget *parent) : OBSDock(parent)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	setFeatures(DockWidgetClosable | DockWidgetMovable |
		    DockWidgetFloatable);
	// Setting a fixed object name is crucial for OBS to be able to restore
	// the docks position, if the dock is not floating
	setObjectName("Adv-ss-dock");

	QAction *action = new QAction;
	action->setProperty("themeID", QVariant(QString::fromUtf8("cogsIcon")));
	action->connect(action, &QAction::triggered, OpenSettingsWindow);
	QIcon icon;
	icon.addFile(
		QString::fromUtf8(":/settings/images/settings/advanced.svg"),
		QSize(), QIcon::Normal, QIcon::Off);
	action->setIcon(icon);

	auto toolLayout = new QBoxLayout(QBoxLayout::TopToBottom, this);
	toolLayout->setContentsMargins(0, 0, 0, 0);
	auto toolbar = new QToolBar;
	toolbar->setIconSize({16, 16});
	toolbar->setFloatable(false);
	toolbar->addAction(action);
	toolLayout->addWidget(toolbar);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(new StatusControl(this));
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(toolLayout);

	// QFrame wrapper is necessary to avoid dock being partially
	// transparent
	QFrame *wrapper = new QFrame;
	wrapper->setFrameShape(QFrame::StyledPanel);
	wrapper->setFrameShadow(QFrame::Sunken);
	wrapper->setLayout(layout);
	setWidget(wrapper);
}

void SetupDock()
{
	dock = new StatusDock(
		static_cast<QMainWindow *>(obs_frontend_get_main_window()));
	// Added for cosmetic reasons to avoid brief flash of dock window on startup
	dock->setVisible(false);
	static_cast<QAction *>(obs_frontend_add_dock(dock));
}
