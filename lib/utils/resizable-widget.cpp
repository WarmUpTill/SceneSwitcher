#include "resizable-widget.hpp"

namespace advss {

ResizableWidget::ResizableWidget(QWidget *parent) : QWidget(parent) {}

void ResizableWidget::SetResizingEnabled(bool enable)
{
	setMouseTracking(enable);
	_customHeight = enable ? sizeHint().height() : 0;
	auto pol = sizePolicy();
	pol.setVerticalPolicy(enable ? QSizePolicy::Fixed
				     : QSizePolicy::Preferred);
	_resizingEnabled = enable;
}

void ResizableWidget::mousePressEvent(QMouseEvent *event)
{
	if (!_resizingEnabled) {
		QWidget::mousePressEvent(event);
		return;
	}

	if (IsInResizeArea(event->pos())) {
		_resizing = true;
		_lastMousePos = event->globalPosition().toPoint();
		event->accept();
	} else {
		QWidget::mousePressEvent(event);
	}
}

void ResizableWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (!_resizingEnabled) {
		QWidget::mouseMoveEvent(event);
		return;
	}

	if (_resizing) {
		const int dy = event->globalPosition().toPoint().y() -
			       _lastMousePos.y();
		const int baseHeight = _customHeight == 0 ? height()
							  : _customHeight;
		const int newHeight = baseHeight + dy;
		_customHeight =
			std::clamp(newHeight, minimumHeight(), maximumHeight());
		updateGeometry();
		_lastMousePos = event->globalPosition().toPoint();
		event->accept();
	} else {
		setCursor(IsInResizeArea(event->pos()) ? Qt::SizeVerCursor
						       : Qt::ArrowCursor);
		QWidget::mouseMoveEvent(event);
	}
}

void ResizableWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (_resizing) {
		event->accept();
		_resizing = false;
	} else {
		QWidget::mouseReleaseEvent(event);
	}
}

void ResizableWidget::paintEvent(QPaintEvent *event)
{
	QWidget::paintEvent(event);

	if (!_resizingEnabled) {
		return;
	}

	// Draw a triangle in bottom-right as resize indicator
	QPainter painter(this);
	QPolygon triangle;
	triangle << QPoint(width() - 1, height() - _gripSize)
		 << QPoint(width() - _gripSize, height() - 1)
		 << QPoint(width() - 1, height() - 1);
	painter.setBrush(Qt::gray);
	painter.drawPolygon(triangle);
}

QSize ResizableWidget::sizeHint() const
{
	if (!_resizingEnabled) {
		return QWidget::sizeHint();
	}

	if (_customHeight == 0) {
		return QWidget::sizeHint();
	}

	return QSize(width(), _customHeight);
}

bool ResizableWidget::IsInResizeArea(const QPoint &pos) const
{
	return QRect(width() - _gripSize, height() - _gripSize, _gripSize,
		     _gripSize)
		.contains(pos);
}

} // namespace advss
