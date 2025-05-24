#include "monitor-helpers.hpp"
#include "obs-module-helper.hpp"

#include <QGuiApplication>
#include <QScreen>
#include <QString>

namespace advss {

QStringList GetMonitorNames()
{
	QStringList monitorNames;
	QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); i++) {
		QScreen *screen = screens[i];
		QRect screenGeometry = screen->geometry();
		qreal ratio = screen->devicePixelRatio();
		QString name = "";
#if defined(__APPLE__) || defined(_WIN32)
		name = screen->name();
#else
		name = screen->model().simplified();
		if (name.length() > 1 && name.endsWith("-")) {
			name.chop(1);
		}
#endif
		name = name.simplified();

		if (name.length() == 0) {
			name = QString("%1 %2")
				       .arg(obs_module_text(
					       "AdvSceneSwitcher.action.projector.display"))
				       .arg(QString::number(i + 1));
		}
		QString str =
			QString("%1: %2x%3 @ %4,%5")
				.arg(name,
				     QString::number(screenGeometry.width() *
						     ratio),
				     QString::number(screenGeometry.height() *
						     ratio),
				     QString::number(screenGeometry.x()),
				     QString::number(screenGeometry.y()));

		monitorNames << str;
	}
	return monitorNames;
}

MonitorSelectionWidget::MonitorSelectionWidget(QWidget *parent)
	: FilterComboBox(parent)
{
	setEditable(true);
	SetAllowUnmatchedSelection(true);
	setMaxVisibleItems(20);
	addItems(GetMonitorNames());
}

void MonitorSelectionWidget::showEvent(QShowEvent *event)
{
	FilterComboBox::showEvent(event);
	const QSignalBlocker b(this);
	const auto text = currentText();
	clear();
	addItems(GetMonitorNames());
	setCurrentText(text);
}

} // namespace advss
