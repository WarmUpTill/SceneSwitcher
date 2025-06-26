#include "macro-segment-list.hpp"
#include "layout-helpers.hpp"
#include "ui-helpers.hpp"

#include <QEvent>
#include <QDrag>
#include <QGridLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSpacerItem>
#include <QtGlobal>

namespace advss {

bool MacroSegmentList::_useCache = true;

MacroSegmentList::MacroSegmentList(QWidget *parent)
	: QScrollArea(parent),
	  _stackedWidget(new QStackedWidget(this)),
	  _layout(new QVBoxLayout),
	  _contentLayout(new QVBoxLayout),
	  _helpMsg(new QLabel(this))
{
	_helpMsg->setWordWrap(true);
	_helpMsg->setAlignment(Qt::AlignCenter);
	auto helpWidget = new QWidget(this);
	auto helpLayout = new QVBoxLayout(helpWidget);
	helpLayout->addWidget(_helpMsg);
	helpLayout->setAlignment(Qt::AlignCenter);

	auto contentWidget = new QWidget(this);
	contentWidget->setLayout(_contentLayout);
	_contentLayout->setSpacing(0);

	auto contentWrapper = new QWidget(this);
	auto wrapperLayout = new QVBoxLayout(contentWrapper);
	wrapperLayout->setContentsMargins(0, 0, 0, 0);
	wrapperLayout->addWidget(contentWidget);
	// Move macro segments to the top of the list
	wrapperLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
					       QSizePolicy::Expanding));

	_stackedWidget->addWidget(helpWidget);
	_stackedWidget->addWidget(contentWrapper);

	setWidget(_stackedWidget);
	setWidgetResizable(true);
	setAcceptDrops(true);

	SetHelpMsgVisible(true);

	connect(verticalScrollBar(), &QScrollBar::valueChanged, this,
		[this]() { SetupVisibleMacroSegmentWidgets(); });
}

static void clearWidgetVector(const std::vector<QWidget *> &widgets)
{
	for (auto widget : widgets) {
		widget->deleteLater();
	}
};

MacroSegmentList::~MacroSegmentList()
{
	if (_autoScrollThread.joinable()) {
		_autoScroll = false;
		_autoScrollThread.join();
	}

	ClearWidgetCache();
}

static bool posIsInScrollbar(const QScrollBar *scrollbar, const QPoint &pos)
{
	if (!scrollbar) {
		return false;
	}
	if (!scrollbar->isVisible()) {
		return false;
	}
	const auto &geo = scrollbar->geometry();
	const auto globalGeo = QRect(scrollbar->mapToGlobal(geo.topLeft()),
				     scrollbar->mapToGlobal(geo.bottomRight()));
	return globalGeo.contains(pos);
}

int MacroSegmentList::GetDragIndex(const QPoint &pos) const
{
	// Don't drag widget when interacting with the scrollbars
	if (posIsInScrollbar(horizontalScrollBar(), mapTo(this, pos)) ||
	    posIsInScrollbar(verticalScrollBar(), mapTo(this, pos))) {
		return -1;
	}

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

void MacroSegmentList::SetHelpMsg(const QString &msg) const
{
	_helpMsg->setText(msg);
}

void MacroSegmentList::SetHelpMsgVisible(bool visible)
{
	_stackedWidget->setCurrentIndex(visible ? 0 : 1);
	adjustSize();
	updateGeometry();
}

void MacroSegmentList::Insert(int idx, QWidget *widget)
{
	widget->installEventFilter(this);
	_contentLayout->insertWidget(idx, widget);
	SetupVisibleMacroSegmentWidgets();
	SetHelpMsgVisible(false);
	adjustSize();
	updateGeometry();
}

void MacroSegmentList::Add(QWidget *widget)
{
	Insert(_contentLayout->count(), widget);
}

void MacroSegmentList::Remove(int idx)
{
	DeleteLayoutItemWidget(_contentLayout->takeAt(idx));
	adjustSize();
	updateGeometry();
	if (IsEmpty()) {
		SetHelpMsgVisible(true);
	}
}

void MacroSegmentList::Clear(int idx)
{
	ClearLayout(_contentLayout, idx);
	adjustSize();
	updateGeometry();

	SetHelpMsgVisible(true);
}

void MacroSegmentList::SetCachingEnabled(bool enable)
{
	_useCache = enable;
}

void MacroSegmentList::CacheCurrentWidgetsFor(const Macro *macro)
{
	if (!_useCache) {
		return;
	}

	std::vector<QWidget *> result;
	int idx = 0;
	QLayoutItem *item;
	while ((item = _contentLayout->takeAt(idx))) {
		if (!item || !item->widget()) {
			continue;
		}
		auto widget = item->widget();
		widget->hide();
		result.emplace_back(widget);
	}

	_widgetCache[macro] = result;
}

bool MacroSegmentList::PopulateWidgetsFromCache(const Macro *macro)
{
	if (!_useCache) {
		return false;
	}

	auto it = _widgetCache.find(macro);
	if (it == _widgetCache.end()) {
		return false;
	}

	for (auto widget : it->second) {
		_contentLayout->addWidget(widget);
		widget->show();
	}

	adjustSize();
	updateGeometry();
	return true;
}

void MacroSegmentList::ClearWidgetsFromCacheFor(const Macro *macro)
{
	auto it = _widgetCache.find(macro);
	if (it == _widgetCache.end()) {
		return;
	}
	clearWidgetVector(it->second);
	_widgetCache.erase(it);
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
	HighlightWidget(widget, color, QColor(0, 0, 0, 0), true);
}

void MacroSegmentList::SetCollapsed(bool collapse) const
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

void MacroSegmentList::SetSelection(int idx) const
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
	if (event->button() == Qt::LeftButton ||
	    event->button() == Qt::RightButton) {
		_dragPosition = GetDragIndex(event->globalPosition().toPoint());
		emit SelectionChanged(_dragPosition);
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

MacroSegmentEdit *MacroSegmentList::WidgetAt(int idx) const
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

MacroSegmentEdit *MacroSegmentList::WidgetAt(const QPoint &pos) const
{
	return WidgetAt(GetSegmentIndexFromPos(mapToGlobal(pos)));
}

int MacroSegmentList::IndexAt(const QPoint &pos) const
{
	return GetSegmentIndexFromPos(mapToGlobal(pos));
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

void MacroSegmentList::ClearWidgetCache()
{
	for (const auto &[_, widgets] : _widgetCache) {
		clearWidgetVector(widgets);
	}
}

static bool isInUpperHalfOf(const QPoint &pos, const QRect &rect)
{
	return QRect(rect.topLeft(),
		     QSize(rect.size().width(), rect.size().height() / 2))
		.contains(pos);
}

static bool widgetIsInLayout(QWidget *w, QLayout *l)
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

	_dragCursorPos = (mapToGlobal(event->position().toPoint()));
	CheckDropLine(_dragCursorPos);
}

QRect MacroSegmentList::GetContentItemRectWithPadding(int idx) const
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

void MacroSegmentList::SetVisibilityCheckEnable(bool enable)
{
	_checkVisibility = enable;

	if (_checkVisibility) {
		QTimer::singleShot(0, this, [this]() {
			SetupVisibleMacroSegmentWidgets();
		});
	}
}

bool MacroSegmentList::IsEmpty() const
{
	return _contentLayout->count() == 0;
}

QSize MacroSegmentList::minimumSizeHint() const
{
	return _stackedWidget->currentWidget()->minimumSizeHint();
}

QSize MacroSegmentList::sizeHint() const
{
	const auto contentSize = _stackedWidget->currentWidget()->sizeHint();
	const auto hint =
		QSize(width(), contentSize.height() + 2 * frameWidth());
	return hint;
}

void MacroSegmentList::SetupVisibleMacroSegmentWidgets()
{
	if (!_checkVisibility) {
		return;
	}

	const auto viewportRect = viewport()->rect();

	for (auto segment : widget()->findChildren<MacroSegmentEdit *>()) {
		const auto pos = segment->mapTo(viewport(), QPoint(0, 0));
		const QRect rect(pos, segment->size());
		if (!viewportRect.intersects(rect)) {
			continue;
		}

		segment->SetupWidgets();
	}
	updateGeometry();
}

int MacroSegmentList::GetSegmentIndexFromPos(const QPoint &pos) const
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

void MacroSegmentList::CheckScroll() const
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
	int idx = GetSegmentIndexFromPos(pos);
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

bool MacroSegmentList::IsInListArea(const QPoint &pos) const
{
	const QRect layoutRect(mapToGlobal(_layout->contentsRect().topLeft()),
			       _layout->contentsRect().size());
	return layoutRect.contains(pos);
}

int MacroSegmentList::GetDropIndex(const QPoint &pos) const
{
	int idx = GetSegmentIndexFromPos(pos);
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
	_dragPosition = -1;
}

void MacroSegmentList::resizeEvent(QResizeEvent *event)
{
	QScrollArea::resizeEvent(event);
	SetupVisibleMacroSegmentWidgets();
}

} // namespace advss
