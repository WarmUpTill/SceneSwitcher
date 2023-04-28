#include "macro-segment-list.hpp"
#include "utility.hpp"

#include <QGridLayout>
#include <QSpacerItem>
#include <QEvent>
#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QScrollBar>
#include <QtGlobal>

namespace advss {

MacroSegmentList::MacroSegmentList(QWidget *parent)
	: QScrollArea(parent),
	  _layout(new QVBoxLayout),
	  _contentLayout(new QVBoxLayout),
	  _helpMsg(new QLabel)
{
	_helpMsg->setWordWrap(true);
	_helpMsg->setAlignment(Qt::AlignCenter);
	_contentLayout->setSpacing(0);
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

MacroSegmentList::~MacroSegmentList()
{
	if (_autoScrollThread.joinable()) {
		_autoScroll = false;
		_autoScrollThread.join();
	}
}

int MacroSegmentList::GetDragIndex(const QPoint &pos)
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
		if (rect.contains(pos)) {
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

void MacroSegmentList::Insert(int idx, MacroSegmentEdit *widget)
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
	DeleteLayoutItemWidget(_contentLayout->takeAt(idx));
}

void MacroSegmentList::Clear(int idx)
{
	ClearLayout(_contentLayout, idx);
}

void MacroSegmentList::Highlight(int idx, QColor color)
{
	auto item = _contentLayout->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, color, QColor(0, 0, 0, 0), true);
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		_dragPosition = GetDragIndex(event->globalPos());
#else
		_dragPosition = GetDragIndex(event->globalPosition().toPoint());
#endif
		emit SelectionChagned(_dragPosition);
	} else {
		_dragPosition = -1;
	}
}

void MacroSegmentList::mouseMoveEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::LeftButton && _dragPosition != -1) {
		auto item = _contentLayout->itemAt(_dragPosition);
		if (!item) {
			return;
		}
		auto widget = item->widget();
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
		_autoScroll = true;
		_autoScrollThread =
			std::thread(&MacroSegmentList::CheckScroll, this);
		drag->exec();
		_autoScroll = false;
		_autoScrollThread.join();
	}
}

void MacroSegmentList::mouseReleaseEvent(QMouseEvent *)
{
	_dragPosition = -1;
}

void MacroSegmentList::dragLeaveEvent(QDragLeaveEvent *)
{
	HideLastDropLine();
}

void MacroSegmentList::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData() && event->mimeData()->hasImage()) {
		event->accept();
	} else {
		event->ignore();
	}
}

MacroSegmentEdit *MacroSegmentList::WidgetAt(int idx)
{
	if (idx < 0 || idx >= _contentLayout->count()) {
		return nullptr;
	}
	auto item = _contentLayout->itemAt(idx);
	if (!item) {
		return nullptr;
	}
	return static_cast<MacroSegmentEdit *>(item->widget());
}

void MacroSegmentList::HideLastDropLine()
{
	if (_dropLineIdx >= 0 && _dropLineIdx < _contentLayout->count()) {
		auto widget = WidgetAt(_dropLineIdx);
		if (widget) {
			widget->ShowDropLine(
				MacroSegmentEdit::DropLineState::NONE);
		}
	}
	_dropLineIdx = -1;
}

bool isInUpperHalfOf(const QPoint &pos, const QRect &rect)
{
	return QRect(rect.topLeft(),
		     QSize(rect.size().width(), rect.size().height() / 2))
		.contains(pos);
}

bool widgetIsInLayout(QWidget *w, QLayout *l)
{
	if (w == nullptr) {
		return false;
	}
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

void MacroSegmentList::dragMoveEvent(QDragMoveEvent *event)
{
	auto widget = qobject_cast<QWidget *>(event->source());
	if (!widgetIsInLayout(widget, _contentLayout)) {
		return;
	}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	_dragCursorPos = (mapToGlobal(event->pos()));
#else
	_dragCursorPos = (mapToGlobal(event->position().toPoint()));
#endif
	CheckDropLine(_dragCursorPos);
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
	const QRect rect(
		mapToGlobal(QPoint(itemRect.topLeft().x(),
				   itemRect.topLeft().y() -
					   _contentLayout->spacing() -
					   scrollOffset)),
		QSize(itemRect.size().width(),
		      itemRect.size().height() + _contentLayout->spacing()));
	return rect;
}

int MacroSegmentList::GetWidgetIdx(const QPoint &pos)
{
	int idx = -1;
	for (int i = 0; i < _contentLayout->count(); ++i) {
		if (GetContentItemRectWithPadding(i).contains(pos)) {
			idx = i;
			break;
		}
	}
	return idx;
}

void MacroSegmentList::CheckScroll()
{
	while (_autoScroll) {
		const int scrollTrigger = 15;
		const int scrollAmount = 1;
		const QRect rect(mapToGlobal(QPoint(0, 0)), size());
		const QRect upperScrollTrigger(
			QPoint(rect.topLeft().x(),
			       rect.topLeft().y() - scrollTrigger),
			QSize(rect.width(), scrollTrigger * 2));
		if (upperScrollTrigger.contains(_dragCursorPos)) {
			verticalScrollBar()->setValue(
				verticalScrollBar()->value() - scrollAmount);
		}
		const QRect lowerScrollTrigger(
			QPoint(rect.bottomLeft().x(),
			       rect.bottomLeft().y() - scrollTrigger),
			QSize(rect.width(), scrollTrigger * 2));
		if (lowerScrollTrigger.contains(_dragCursorPos)) {
			verticalScrollBar()->setValue(
				verticalScrollBar()->value() + scrollAmount);
		}
		std::this_thread::sleep_for(std::chrono::microseconds(50));
	}
}

void MacroSegmentList::CheckDropLine(const QPoint &pos)
{
	int idx = GetWidgetIdx(pos);
	if (idx == _dragPosition) {
		return;
	}
	auto action = MacroSegmentEdit::DropLineState::ABOVE;
	if (idx == -1) {
		if (IsInListArea(pos)) {
			idx = _contentLayout->count() - 1;
			action = MacroSegmentEdit::DropLineState::BELOW;
		} else {
			HideLastDropLine();
			return;
		}
	} else {
		auto rect = GetContentItemRectWithPadding(idx);
		if (idx == _contentLayout->count() - 1 &&
		    !isInUpperHalfOf(pos, rect)) {
			action = MacroSegmentEdit::DropLineState::BELOW;
		} else {
			if (!isInUpperHalfOf(pos, rect)) {
				idx++;
			}
		}
	}
	if (idx == _dragPosition ||
	    (idx - 1 == _dragPosition &&
	     action != MacroSegmentEdit::DropLineState::BELOW)) {
		HideLastDropLine();
		return;
	}
	auto widget = WidgetAt(idx);
	if (!widget) {
		HideLastDropLine();
		return;
	}
	widget->ShowDropLine(action);
	if (_dropLineIdx != idx) {
		HideLastDropLine();
		_dropLineIdx = idx;
	}
}

bool MacroSegmentList::IsInListArea(const QPoint &pos)
{
	const QRect layoutRect(mapToGlobal(_layout->contentsRect().topLeft()),
			       _layout->contentsRect().size());
	return layoutRect.contains(pos);
}

int MacroSegmentList::GetDropIndex(const QPoint &pos)
{
	int idx = GetWidgetIdx(pos);
	if (idx == _dragPosition) {
		return -1;
	}
	if (idx == -1) {
		if (IsInListArea(pos)) {
			return _contentLayout->count() - 1;
		}
		return -1;
	}
	auto rect = GetContentItemRectWithPadding(idx);
	if (idx == _contentLayout->count() - 1 && !isInUpperHalfOf(pos, rect)) {
		return idx;
	} else if (!isInUpperHalfOf(pos, rect)) {
		idx++;
	}
	if (_dragPosition < idx) {
		idx--;
	}
	if (idx == _dragPosition) {
		return -1;
	}
	return idx;
}

void MacroSegmentList::dropEvent(QDropEvent *event)
{
	HideLastDropLine();
	auto widget = qobject_cast<QWidget *>(event->source());
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if (widget && !widget->geometry().contains(event->pos()) &&
	    widgetIsInLayout(widget, _contentLayout)) {
		int dropPosition = GetDropIndex(mapToGlobal(event->pos()));
		if (dropPosition == -1) {
			return;
		}
		emit Reorder(dropPosition, _dragPosition);
	}
#else
	if (widget &&
	    !widget->geometry().contains(event->position().toPoint()) &&
	    widgetIsInLayout(widget, _contentLayout)) {
		int dropPosition =
			GetDropIndex(mapToGlobal(event->position().toPoint()));
		if (dropPosition == -1) {
			return;
		}
		emit Reorder(dropPosition, _dragPosition);
	}
#endif
	_dragPosition = -1;
}

} // namespace advss
