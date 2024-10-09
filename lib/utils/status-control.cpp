#include "status-control.hpp"
#include "log-helper.hpp"
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

#if LIBOBS_API_VER < MAKE_SEMANTIC_VERSION(30, 0, 0)
#include <QDockWidget>

static bool obs_frontend_add_dock_by_id(const char *id, const char *title,
					QWidget *widget)
{
	widget->setObjectName(id);

	auto dock = new QDockWidget();
	dock->setWindowTitle(title);
	dock->setWidget(widget);
	dock->setFloating(true);
	dock->setVisible(false);
	dock->setFeatures(QDockWidget::DockWidgetClosable |
			  QDockWidget::DockWidgetMovable |
			  QDockWidget::DockWidgetFloatable);

	return !!obs_frontend_add_dock(dock);
}
#endif

namespace advss {

void OpenSettingsWindow();

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
	if (_pulse) {
		_pulse->deleteLater();
		_pulse = nullptr;
	}
	SetStatusStyleSheet(false);
	_setToStopped = false;
}

void StatusControl::SetStopped()
{
	_button->setText(
		obs_module_text("AdvSceneSwitcher.generalTab.status.start"));
	_status->setText(obs_module_text("AdvSceneSwitcher.status.inactive"));
	SetStatusStyleSheet(true);
	if (HighlightUIElementsEnabled()) {
		_pulse = HighlightWidget(_status, QColor(Qt::red),
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

StatusDockWidget::StatusDockWidget(QWidget *parent) : QFrame(parent)
{
	setFrameShape(QFrame::StyledPanel);
	setFrameShadow(QFrame::Sunken);

	auto action = new QAction;
	action->setProperty("themeID", QVariant(QString::fromUtf8("cogsIcon")));
	action->setProperty("class", QVariant(QString::fromUtf8("icon-cogs")));
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

	auto layout = new QVBoxLayout;
	layout->addWidget(statusControl);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
}

void SetupDock()
{
	auto dock = new StatusDockWidget();
	if (!obs_frontend_add_dock_by_id(
		    "advss-status-dock",
		    obs_module_text("AdvSceneSwitcher.windowTitle"), dock)) {
		blog(LOG_INFO, "failed to register status dock!");
		dock->deleteLater();
	}
}

} // namespace advss
