#include "macro-dock-window.hpp"
#include "log-helper.hpp"

#include <QDockWidget>
#include <QLayout>

// TODO: remove
#include <QLabel>

namespace advss {

MacroDockWindow::MacroDockWindow() : QFrame(), _window(new QMainWindow())
{
	_window->setDockNestingEnabled(true);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_window);
	layout->addWidget(new QLabel("asdf"));
	setLayout(layout);
}

QWidget *MacroDockWindow::AddMacroDock(QWidget *widget, const QString &title)
{
	QDockWidget *dock = new QDockWidget(title, _window);
	dock->setWidget(widget);
	dock->setWindowTitle(title);
	//dock->setObjectName(QT_UTF8(id));
	//_window->addDockWidget(Qt::RightDockWidgetArea, dock);
	return dock;
}

void MacroDockWindow::RemoveMacroDock(QWidget *) {}

} // namespace advss
