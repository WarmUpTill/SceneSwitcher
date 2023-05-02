#include "mouse-wheel-guard.hpp"
#include "osc-helpers.hpp"

#include <QEvent>
#include <QScrollBar>

namespace advss {

MouseWheelWidgetAdjustmentGuard::MouseWheelWidgetAdjustmentGuard(QObject *parent)
	: QObject(parent)
{
}

bool MouseWheelWidgetAdjustmentGuard::eventFilter(QObject *o, QEvent *e)
{
	const QWidget *widget = static_cast<QWidget *>(o);
	if (e->type() == QEvent::Wheel && widget && !widget->hasFocus()) {
		e->ignore();
		return true;
	}

	return QObject::eventFilter(o, e);
}

void PreventMouseWheelAdjustWithoutFocus(QWidget *w)
{
	// Ignore QScrollBar as there is no danger of accidentally modifying anything
	// and long expanded QComboBox would be difficult to interact with otherwise.
	// Ignore OSCMessageElementEdit to allow OSCMessageEdit list to up update
	// current index correctly.
	if (qobject_cast<QScrollBar *>(w) ||
	    qobject_cast<OSCMessageElementEdit *>(w)) {
		return;
	}
	w->setFocusPolicy(Qt::StrongFocus);
	w->installEventFilter(new MouseWheelWidgetAdjustmentGuard(w));
}

} // namespace advss
