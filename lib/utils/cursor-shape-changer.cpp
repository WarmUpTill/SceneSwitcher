#include "cursor-shape-changer.hpp"

#include <QCursor>
#include <QEvent>
#include <QGuiApplication>

namespace advss {

CursorShapeChanger::CursorShapeChanger(QObject *parent, Qt::CursorShape shape)
	: QObject(parent),
	  _shape(shape)
{
}

bool CursorShapeChanger::eventFilter(QObject *o, QEvent *e)
{
	if ((e->type() == QEvent::HoverEnter || e->type() == QEvent::Enter) &&
	    !_overrideActive) {
		_overrideActive = true;
		QGuiApplication::setOverrideCursor(QCursor(_shape));
	} else if (_overrideActive && e->type() == QEvent::Leave) {
		_overrideActive = false;
		QGuiApplication::restoreOverrideCursor();
	}

	return QObject::eventFilter(o, e);
}

void SetCursorOnWidgetHover(QWidget *widget, Qt::CursorShape shape)
{
	widget->installEventFilter(new CursorShapeChanger(widget, shape));
}

} // namespace advss
