#include "switch-button.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPalette>

namespace advss {

static const int s_height = 20;
static const int s_innerMargin = 4;
static const int s_handleSize = s_height - s_innerMargin * 2;
static const int s_width = s_handleSize * 2 + s_innerMargin * 2;

SwitchButton::SwitchButton(QWidget *parent) : QWidget{parent}
{
	setSizePolicy({QSizePolicy::Fixed, QSizePolicy::Fixed});
	setFocusPolicy(Qt::TabFocus);
	setAttribute(Qt::WA_Hover);
}

void SwitchButton::setChecked(bool checked)
{
	if (_checked == checked) {
		return;
	}
	_checked = checked;
	emit toggled(checked);
	update();
}

bool SwitchButton::isChecked() const
{
	return _checked;
}

void SwitchButton::toggle()
{
	setChecked(!_checked);
}

QSize SwitchButton::sizeHint() const
{
	return QSize(s_width, s_height);
}

void SwitchButton::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	QPalette pal = palette();

	if (!isEnabled()) {
		painter.setPen(pal.color(QPalette::Midlight));
		painter.setOpacity(0.5);
	} else if (_mouseDown) {
		painter.setPen(pal.color(QPalette::Light));
	} else if (underMouse() || hasFocus()) {
		painter.setPen(QPen(pal.brush(QPalette::Highlight), 1));
	} else {
		painter.setPen(pal.color(QPalette::Midlight));
	}

	if (_checked) {
		painter.setBrush(pal.color(QPalette::Button));
	}
	const qreal radius = height() / 2;
	painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
				radius, radius);

	// Now draw the handle
	QRect valueRect = rect().adjusted(s_innerMargin, s_innerMargin,
					  -s_innerMargin, -s_innerMargin);
	valueRect.setWidth(valueRect.height());

	if (_checked) {
		valueRect.moveLeft(width() / 2);
	}
	painter.setBrush(pal.color(QPalette::Base));
	painter.drawEllipse(valueRect);
}

void SwitchButton::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		_mouseDown = true;
	} else {
		event->ignore();
	}
}

void SwitchButton::mouseReleaseEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton && _mouseDown) {
		_mouseDown = false;
		toggle();
		emit checked(_checked);
	} else {
		event->ignore();
	}
}

} // namespace advss
