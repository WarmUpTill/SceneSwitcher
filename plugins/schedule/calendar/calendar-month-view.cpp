#include "calendar-month-view.hpp"
#include "obs-module-helper.hpp"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

namespace advss {

CalendarMonthView::CalendarMonthView(QWidget *parent) : CalendarView(parent)
{
	setMinimumSize(320, 240);
	SetDate(QDate::currentDate());
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void CalendarMonthView::SetDate(const QDate &date)
{
	_currentDate = date;

	// Grid starts on the Monday of the week that contains the 1st of the month.
	const QDate first(date.year(), date.month(), 1);
	// Qt: dayOfWeek() -> 1=Mon … 7=Sun
	_gridStart = first.addDays(-(first.dayOfWeek() - 1));

	update();
	emit VisibleRangeChanged(RangeStart(), RangeEnd());
}

void CalendarMonthView::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	update();
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

QDate CalendarMonthView::CellDate(int row, int col) const
{
	return _gridStart.addDays(row * COLS + col);
}

QRect CalendarMonthView::CellRect(int row, int col) const
{
	const int cellW = width() / COLS;
	const int cellH = (height() - HEADER_HEIGHT) / ROWS;
	return QRect(col * cellW, HEADER_HEIGHT + row * cellH, cellW, cellH);
}

bool CalendarMonthView::CellFromPoint(const QPoint &pos, int &row,
				      int &col) const
{
	if (pos.y() < HEADER_HEIGHT) {
		return false;
	}
	const int cellW = width() / COLS;
	const int cellH = (height() - HEADER_HEIGHT) / ROWS;
	col = pos.x() / cellW;
	row = (pos.y() - HEADER_HEIGHT) / cellH;
	return col >= 0 && col < COLS && row >= 0 && row < ROWS;
}

// ---------------------------------------------------------------------------
// Event helpers
// ---------------------------------------------------------------------------

QList<CalendarEvent> CalendarMonthView::EventsForDate(const QDate &date) const
{
	QList<CalendarEvent> result;
	for (const auto &event : _events) {
		if (!event.start.isValid()) {
			continue;
		}
		const QDate evStart = event.start.date();
		const QDate evEnd = event.EffectiveEnd().date();
		if (date >= evStart && date <= evEnd) {
			result.append(event);
		}
	}
	return result;
}

QString CalendarMonthView::EventIdAtPoint(const QPoint &pos) const
{
	int row, col;
	if (!CellFromPoint(pos, row, col)) {
		return {};
	}

	const QRect cell = CellRect(row, col);
	const int evTop = cell.top() + DAY_NUM_HEIGHT + CELL_PAD;
	const auto events = EventsForDate(CellDate(row, col));

	for (int i = 0; i < qMin((int)events.size(), MAX_VISIBLE_EVENTS); ++i) {
		const QRect evRect(cell.left() + CELL_PAD,
				   evTop + i * (EVENT_HEIGHT + EVENT_MARGIN),
				   cell.width() - CELL_PAD * 2, EVENT_HEIGHT);
		if (evRect.contains(pos)) {
			return events[i].id;
		}
	}
	return {};
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void CalendarMonthView::PaintDayNameHeader(QPainter &p)
{
	p.fillRect(0, 0, width(), HEADER_HEIGHT, palette().button());

	const int cellW = width() / COLS;
	QLocale locale;

	p.setPen(palette().buttonText().color());
	QFont f = p.font();
	f.setPixelSize(11);
	p.setFont(f);

	for (int col = 0; col < COLS; ++col) {
		// dayOfWeek: 1=Mon … 7=Sun (ISO 8601 order matches our columns)
		const QString name =
			locale.dayName(col + 1, QLocale::ShortFormat);
		p.drawText(col * cellW, 0, cellW, HEADER_HEIGHT,
			   Qt::AlignCenter, name);
	}

	// Bottom border
	p.setPen(palette().mid().color());
	p.drawLine(0, HEADER_HEIGHT - 1, width(), HEADER_HEIGHT - 1);
}

void CalendarMonthView::PaintCell(QPainter &p, int row, int col)
{
	const QRect rect = CellRect(row, col);
	const QDate date = CellDate(row, col);
	const QDate today = QDate::currentDate();
	const bool isToday = (date == today);
	const bool inCurrentMonth = (date.month() == _currentDate.month());

	// --- Background ---
	QColor bg;
	if (isToday) {
		bg = palette().highlight().color().lighter(185);
	} else if (!inCurrentMonth) {
		bg = palette().alternateBase().color();
	} else {
		bg = palette().base().color();
	}
	// Weekend tint
	if (col >= 5) {
		bg = bg.darker(104);
	}
	p.fillRect(rect, bg);

	// --- Grid border ---
	p.setBrush(Qt::NoBrush);
	p.setPen(palette().mid().color());
	p.drawRect(rect.adjusted(0, 0, -1, -1));

	// --- Day number ---
	const QRect numArea(rect.left(), rect.top() + CELL_PAD,
			    rect.width() - CELL_PAD, DAY_NUM_HEIGHT);

	if (isToday) {
		// Filled circle behind the number
		const int dia = DAY_NUM_HEIGHT - 4;
		const QRect circleRect(numArea.right() - dia - 2,
				       numArea.top() + 1, dia, dia);
		p.setBrush(palette().highlight());
		p.setPen(Qt::NoPen);
		p.drawEllipse(circleRect);
		p.setPen(palette().highlightedText().color());
	} else {
		p.setPen(inCurrentMonth ? palette().text().color()
					: palette().placeholderText().color());
	}

	QFont f = p.font();
	f.setPixelSize(11);
	f.setBold(isToday);
	p.setFont(f);
	p.drawText(numArea, Qt::AlignRight | Qt::AlignTop,
		   QString::number(date.day()));

	// --- Event bars ---
	const auto events = EventsForDate(date);
	const int evTop = rect.top() + DAY_NUM_HEIGHT + CELL_PAD;
	const int maxFit = (rect.height() - DAY_NUM_HEIGHT - CELL_PAD * 2) /
			   (EVENT_HEIGHT + EVENT_MARGIN);
	const int visible =
		qMin(qMin((int)events.size(), maxFit), MAX_VISIBLE_EVENTS);

	QFont evFont = p.font();
	evFont.setPixelSize(10);
	evFont.setBold(false);
	p.setFont(evFont);

	for (int i = 0; i < visible; ++i) {
		const auto &ev = events[i];
		const QRect evRect(rect.left() + CELL_PAD,
				   evTop + i * (EVENT_HEIGHT + EVENT_MARGIN),
				   rect.width() - CELL_PAD * 2, EVENT_HEIGHT);

		p.setBrush(ev.color);
		p.setPen(Qt::NoPen);
		p.drawRoundedRect(evRect, 2, 2);

		p.setPen(Qt::white);
		p.drawText(evRect.adjusted(4, 0, -2, 0),
			   Qt::AlignVCenter | Qt::AlignLeft |
				   Qt::TextSingleLine,
			   ev.title);
	}

	// "+N more" overflow label
	if ((int)events.size() > visible) {
		const int extra = events.size() - visible;
		const QRect moreRect(
			rect.left() + CELL_PAD,
			evTop + visible * (EVENT_HEIGHT + EVENT_MARGIN),
			rect.width() - CELL_PAD * 2, EVENT_HEIGHT);
		p.setPen(palette().placeholderText().color());
		p.drawText(
			moreRect,
			Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine,
			QString(obs_module_text(
					"AdvSceneSwitcher.calendar.moreEvents"))
				.arg(extra));
	}
}

void CalendarMonthView::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	PaintDayNameHeader(p);

	for (int row = 0; row < ROWS; ++row) {
		for (int col = 0; col < COLS; ++col) {
			PaintCell(p, row, col);
		}
	}
}

// ---------------------------------------------------------------------------
// Mouse events
// ---------------------------------------------------------------------------

void CalendarMonthView::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const QString id = EventIdAtPoint(e->pos());
	if (!id.isEmpty()) {
		emit EventClicked(id);
		return;
	}
	int row, col;
	if (CellFromPoint(e->pos(), row, col)) {
		emit SlotClicked(QDateTime(CellDate(row, col), QTime(0, 0)));
	}
}

void CalendarMonthView::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const QString id = EventIdAtPoint(e->pos());
	if (!id.isEmpty()) {
		emit EventDoubleClicked(id);
	}
}

} // namespace advss
