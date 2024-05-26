#include "status-control.hpp"
#include "obs-module-helper.hpp"
#include "path-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <obs-frontend-api.h>
#include <QAction>
#include <QMainWindow>
#include <QPalette>
#include <QToolBar>

namespace advss {

void OpenSettingsWindow();
StatusDock *dock = nullptr;

static QString colorToString(const QColor &color)
{
	return QString("rgba(") + QString::number(color.red()) + "," +
	       QString::number(color.green()) + "," +
	       QString::number(color.blue()) + "," +
	       QString::number(color.alpha()) + ")";
}

static QString getDefaultBackgroundColorString()
{
	static QString defaultColorString;
	static bool cacheReady = false;

	if (cacheReady) {
		return defaultColorString;
	}

	QWidget tempWidget;
	const auto defaultColor =
		tempWidget.palette().brush(QPalette::Window).color();
	defaultColorString = colorToString(defaultColor);

	cacheReady = true;
	return defaultColorString;
}

static QString getDefaultTextColorString()
{
	static QString defaultColorString;
	static bool cacheReady = false;

	if (cacheReady) {
		return defaultColorString;
	}

	QLabel tempWidget;
	const auto defaultColor =
		tempWidget.palette().brush(QPalette::Text).color();
	defaultColorString = colorToString(defaultColor);

	cacheReady = true;
	return defaultColorString;
}

StatusControl::StatusControl(QWidget *parent, bool noLayout)
	: QWidget(parent),
	  _button(new QPushButton("-", this)),
	  _buttonLayout(new QHBoxLayout()),
	  _status(new QLabel("-", this)),
	  _statusPrefix(new QLabel(
		  obs_module_text(
			  "AdvSceneSwitcher.generalTab.status.currentStatus"),
		  this))
{
	_statusPrefix->setWordWrap(true);
	_statusPrefix->setSizePolicy(QSizePolicy::Expanding,
				     QSizePolicy::Expanding);
	SetStatusStyleSheet(false);
	QWidget::connect(_button, SIGNAL(clicked()), this,
			 SLOT(ButtonClicked()));

	if (!noLayout) {
		_buttonLayout->setContentsMargins(0, 0, 0, 0);
		_buttonLayout->addWidget(_button);
		QVBoxLayout *layout = new QVBoxLayout();
		_statusPrefix->setAlignment(Qt::AlignCenter);
		_status->setAlignment(Qt::AlignCenter);
		layout->addWidget(_statusPrefix);
		layout->addWidget(_status);
		layout->addLayout(_buttonLayout);
		setLayout(layout);
	} else {
		_status->setSizePolicy(QSizePolicy::Maximum,
				       QSizePolicy::MinimumExpanding);
	}

	if (PluginIsRunning()) {
		SetStarted();
	} else {
		SetStopped();
	}
	connect(&_timer, SIGNAL(timeout()), this, SLOT(UpdateStatus()));
	_timer.start(1000);
}

void StatusControl::ButtonClicked()
{
	if (PluginIsRunning()) {
		StopPlugin();
		SetStopped();
	} else {
		StartPlugin();
		SetStarted();
	}
}

void StatusControl::UpdateStatus()
{
	if (PluginIsRunning()) {
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
	SetStatusStyleSheet(false);

	_setToStopped = false;
}

void StatusControl::SetStopped()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.inactive"));
	if (HighlightUIElementsEnabled()) {
		SetStatusStyleSheet(true);
		_pulse = PulseWidget(_status, QColor(Qt::red),
				     QColor(0, 0, 0, 0));
	}
	_setToStopped = true;
}

void StatusControl::SetStatusStyleSheet(bool stopped) const
{
	auto style = QString("QLabel{ "
			     "background-color: ");
	if (stopped) {
		style += getDefaultBackgroundColorString() + "; ";
	} else {
		style += "transparent; ";
	}
	style += "border-style: outset; "
		 "border-width: 2px; "
		 "border-radius: 7px; "
		 "border-color: " +
		 getDefaultTextColorString() + "; }";
	_status->setStyleSheet(style);
}

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
	const auto path = QString::fromStdString(GetDataFilePath(
		"res/images/" + GetThemeTypeName() + "Advanced.svg"));
	QIcon icon(path);
	action->setIcon(icon);

	auto toolbar = new QToolBar;
	toolbar->setIconSize({16, 16});
	toolbar->setFloatable(false);
	toolbar->addAction(action);

	auto statusControl = new StatusControl(this);
	statusControl->ButtonLayout()->addWidget(toolbar);
	statusControl->ButtonLayout()->setStretchFactor(statusControl->Button(),
							100);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(statusControl);
	layout->setContentsMargins(0, 0, 0, 0);

	// QFrame wrapper is necessary to avoid dock being partially
	// transparent
	QFrame *wrapper = new QFrame;
	wrapper->setFrameShape(QFrame::StyledPanel);
	wrapper->setFrameShadow(QFrame::Sunken);
	wrapper->setLayout(layout);
	setWidget(wrapper);

	setFloating(true);
	hide();
}

void SetupDock()
{
	dock = new StatusDock(
		static_cast<QMainWindow *>(obs_frontend_get_main_window()));
	// Added for cosmetic reasons to avoid brief flash of dock window on startup
	dock->setVisible(false);
	obs_frontend_add_dock(dock);
}

} // namespace advss
