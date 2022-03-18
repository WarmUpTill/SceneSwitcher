#include "headers/macro-segment-list.hpp"
#include "headers/utility.hpp"

#include <QGridLayout>
#include <QSpacerItem>
#include <QEvent>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QScrollBar>

MacroSegmentList::MacroSegmentList(QWidget *parent)
	: QScrollArea(parent),
	  _layout(new QVBoxLayout),
	  _contentLayout(new QVBoxLayout),
	  _helpMsg(new QLabel)
{
	_helpMsg->setWordWrap(true);
	_helpMsg->setAlignment(Qt::AlignCenter);

	auto helperLayout = new QGridLayout();
	helperLayout->addWidget(_helpMsg, 0, 0,
				Qt::AlignHCenter | Qt::AlignVCenter);
	helperLayout->addLayout(_contentLayout, 0, 0);
	helperLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
	_layout->addLayout(helperLayout, 10);
	_layout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
					 QSizePolicy::Expanding));
	auto wrapper = new QWidget;
	wrapper->setLayout(_layout);
	setWidget(wrapper);
	setWidgetResizable(true);
	setAcceptDrops(true);
}

int MacroSegmentList::GetIndex(QPoint pos)
{
	for (int idx = 0; idx < _contentLayout->count(); ++idx) {
		auto item = _contentLayout->itemAt(idx);
		if (!item) {
			continue;
		}
		const auto geo = item->geometry();
		int scrollOffset = 0;
		if (verticalScrollBar()) {
			scrollOffset = verticalScrollBar()->value();
		}
		const QRect rect(
			mapToGlobal(QPoint(geo.topLeft().x(),
					   geo.topLeft().y() - scrollOffset)),
			geo.size());
		if (rect.contains(pos) && idx != _dragPosition) {
			return idx;
		}
	}
	return -1;
}

void MacroSegmentList::SetHelpMsg(const QString &msg)
{
	_helpMsg->setText(msg);
}

void MacroSegmentList::SetHelpMsgVisible(bool visible)
{
	_helpMsg->setVisible(visible);
}

void MacroSegmentList::Insert(int idx, QWidget *widget)
{
	widget->installEventFilter(this);
	_contentLayout->insertWidget(idx, widget);
}

void MacroSegmentList::Add(QWidget *widget)
{
	widget->installEventFilter(this);
	_contentLayout->addWidget(widget);
}

void MacroSegmentList::Remove(int idx)
{
	deleteLayoutItemWidget(_contentLayout->takeAt(idx));
}

void MacroSegmentList::Clear(int idx)
{
	clearLayout(_contentLayout, idx);
}

void MacroSegmentList::Highlight(int idx)
{
	auto item = _contentLayout->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, QColor(Qt::green), QColor(0, 0, 0, 0), true);
}

void MacroSegmentList::SetCollapsed(bool collapse)
{
	QLayoutItem *item = nullptr;
	for (int i = 0; i < _contentLayout->count(); i++) {
		item = _contentLayout->itemAt(i);
		auto segment = dynamic_cast<MacroSegmentEdit *>(item->widget());
		if (segment) {
			segment->SetCollapsed(collapse);
		}
	}
}

void MacroSegmentList::SetSelection(int idx)
{
	for (int i = 0; i < _contentLayout->count(); ++i) {
		auto widget = static_cast<MacroSegmentEdit *>(
			_contentLayout->itemAt(i)->widget());
		if (widget) {
			widget->SetSelected(i == idx);
		}
	}
}

bool MacroSegmentList::eventFilter(QObject *object, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress) {
	}
	switch (event->type()) {
	case QEvent::MouseButtonPress:
		mousePressEvent(static_cast<QMouseEvent *>(event));
		break;
	case QEvent::MouseMove:
		mouseMoveEvent(static_cast<QMouseEvent *>(event));
		break;
	case QEvent::MouseButtonRelease:
		mouseReleaseEvent(static_cast<QMouseEvent *>(event));
		break;
	default:
		break;
	}
	return QWidget::eventFilter(object, event);
}

void MacroSegmentList::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		_dragPosition = GetIndex(event->globalPos());
		emit SelectionChagned(_dragPosition);
	} else {
		_dragPosition = -1;
	}
}

void MacroSegmentList::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton && _dragPosition != -1) {
		QWidget *widget = nullptr;
		if (_contentLayout->itemAt(_dragPosition)) {
			widget =
				_contentLayout->itemAt(_dragPosition)->widget();
		}
		if (!widget) {
			return;
		}
		QDrag *drag = new QDrag(widget);
		auto img = widget->grab();
		auto mimedata = new QMimeData();
		mimedata->setImageData(img);
		drag->setMimeData(mimedata);
		drag->setPixmap(img);
		drag->setHotSpot(event->pos());
		drag->exec();
	}
}

void MacroSegmentList::mouseReleaseEvent(QMouseEvent *)
{
	_dragPosition = -1;
}

void MacroSegmentList::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData() && event->mimeData()->hasImage()) {
		event->accept();
	} else {
		event->ignore();
	}
}

QRect MacroSegmentList::GetContentItemRectWithPadding(int idx)
{
	auto item = _contentLayout->itemAt(idx);
	if (!item) {
		return {};
	}
	int scrollOffset = 0;
	if (verticalScrollBar()) {
		scrollOffset = verticalScrollBar()->value();
	}
	const QRect itemRect = item->geometry().marginsAdded(
		_contentLayout->contentsMargins());
	const QRect rect(mapToGlobal(QPoint(itemRect.topLeft().x(),
					    itemRect.topLeft().y() -
						    _contentLayout->spacing() -
						    scrollOffset)),
			 itemRect.size());
	return rect;
}

int MacroSegmentList::GetDropIndex(QPoint pos)
{
	const QRect layoutRect(mapToGlobal(_layout->contentsRect().topLeft()),
			       _layout->contentsRect().size());
	if (!layoutRect.contains(pos)) {
		return -1;
	}
	int idx = -1;
	for (int i = 0; i < _contentLayout->count(); ++i) {
		if (GetContentItemRectWithPadding(i).contains(pos)) {
			idx = i;
			break;
		}
	}
	if (idx == -1) {
		idx = _contentLayout->count() - 1;
	}
	if (idx != _dragPosition) {
		return idx;
	}
	return -1;
}

bool widgetIsInLayout(QWidget *w, QLayout *l)
{
	for (int i = 0; i < l->count(); ++i) {
		auto item = l->itemAt(i);
		if (!item) {
			continue;
		}
		if (item->widget() == w) {
			return true;
		}
	}
	return false;
}

void MacroSegmentList::dropEvent(QDropEvent *event)
{
	auto widget = qobject_cast<QWidget *>(event->source());
	if (widget && !widget->geometry().contains(event->pos()) &&
	    widgetIsInLayout(widget, _contentLayout)) {
		int dropPosition = GetDropIndex(mapToGlobal(event->pos()));
		if (dropPosition == -1) {
			return;
		}
		emit Reorder(dropPosition, _dragPosition);
	}
	_dragPosition = -1;
}
