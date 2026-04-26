#include "calendar-week-view.hpp"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace advss {

// ---------------------------------------------------------------------------
// CalendarWeekDayHeader – fixed day-name strip above the scroll area
// ---------------------------------------------------------------------------

class CalendarWeekDayHeader : public QWidget {
public:
	explicit CalendarWeekDayHeader(QWidget *parent = nullptr);
	void SetStartOfWeek(const QDate &date);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	QDate _startOfWeek;
};

CalendarWeekDayHeader::CalendarWeekDayHeader(QWidget *parent) : QWidget(parent)
{
	setFixedHeight(CalendarWeekView::DAY_HEADER_HEIGHT);
}

void CalendarWeekDayHeader::SetStartOfWeek(const QDate &date)
{
	_startOfWeek = date;
	update();
}

void CalendarWeekDayHeader::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const int dayW =
		std::max(CalendarWeekView::MIN_DAY_WIDTH,
			 (width() - CalendarWeekView::TIME_AXIS_WIDTH) / 7);
	const QDate today = QDate::currentDate();
	QLocale locale;

	// Time-axis placeholder
	p.fillRect(0, 0, CalendarWeekView::TIME_AXIS_WIDTH, height(),
		   palette().button());

	for (int d = 0; d < 7; ++d) {
		const QDate date = _startOfWeek.addDays(d);
		const int x = CalendarWeekView::TIME_AXIS_WIDTH + d * dayW;
		const bool isToday = (date == today);

		QColor bg = palette().button().color();
		if (isToday) {
			bg = palette().highlight().color().lighter(165);
		} else if (date.dayOfWeek() >= 6) {
			bg = bg.darker(106);
		}
		p.fillRect(x, 0, dayW, height(), bg);

		p.setPen(palette().mid().color());
		p.drawLine(x, 0, x, height());

		QFont f = p.font();
		f.setPixelSize(12);
		f.setBold(isToday);
		p.setFont(f);
		p.setPen(isToday ? palette().highlight().color()
				 : palette().buttonText().color());

		const QString label =
			locale.dayName(date.dayOfWeek(), QLocale::ShortFormat) +
			" " + QString::number(date.day());
		p.drawText(x, 0, dayW, height(), Qt::AlignCenter, label);
	}

	p.setPen(palette().mid().color());
	p.drawLine(0, height() - 1, width(), height() - 1);
}

// ---------------------------------------------------------------------------
// CalendarWeekTimeGrid – scrollable painted time grid
// ---------------------------------------------------------------------------

class CalendarWeekTimeGrid : public QWidget {
	Q_OBJECT

public:
	explicit CalendarWeekTimeGrid(QWidget *parent = nullptr);
	void SetStartOfWeek(const QDate &date);
	void SetEvents(const QList<CalendarEvent> &events);
	void ScrollToCurrentTime(QScrollArea *sa);

	QSize sizeHint() const override;

signals:
	void SlotClicked(const QDateTime &startTime);
	void EventClicked(const QString &id);
	void EventDoubleClicked(const QString &id);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
	void timerEvent(QTimerEvent *) override;

private:
	struct EventLayout {
		CalendarEvent event;
		QTime drawStart; // clipped to the day's [00:00, 23:59:59]
		QTime drawEnd;
		int col;
		int numCols;
	};

	int TimeToY(const QTime &t) const;
	QTime YToTime(int y) const;
	int DayWidth() const;
	int DayColumnX(int dayIndex) const;

	QList<EventLayout> LayoutDay(int dayIndex) const;
	QString EventIdAtPoint(const QPoint &pos) const;
	QDateTime SlotAtPoint(const QPoint &pos) const;

	void DrawEvent(QPainter &p, const EventLayout &layout, int dayX,
		       int dayW);

	QDate _startOfWeek;
	QList<CalendarEvent> _events;
	int _refreshTimerId = 0;
};

CalendarWeekTimeGrid::CalendarWeekTimeGrid(QWidget *parent) : QWidget(parent)
{
	_refreshTimerId = startTimer(60 * 1000);
}

QSize CalendarWeekTimeGrid::sizeHint() const
{
	return QSize(CalendarWeekView::TIME_AXIS_WIDTH +
			     7 * CalendarWeekView::MIN_DAY_WIDTH,
		     24 * CalendarWeekView::PIXELS_PER_HOUR);
}

void CalendarWeekTimeGrid::timerEvent(QTimerEvent *e)
{
	if (e->timerId() == _refreshTimerId) {
		update();
	}
}

int CalendarWeekTimeGrid::TimeToY(const QTime &t) const
{
	return (t.hour() * 60 + t.minute()) *
	       CalendarWeekView::PIXELS_PER_HOUR / 60;
}

QTime CalendarWeekTimeGrid::YToTime(int y) const
{
	int totalMin = y * 60 / CalendarWeekView::PIXELS_PER_HOUR;
	totalMin = qBound(0, totalMin, 24 * 60 - 1);
	totalMin = (totalMin / 15) * 15; // snap to 15-min increments
	return QTime(totalMin / 60, totalMin % 60);
}

int CalendarWeekTimeGrid::DayWidth() const
{
	return std::max(CalendarWeekView::MIN_DAY_WIDTH,
			(width() - CalendarWeekView::TIME_AXIS_WIDTH) / 7);
}

int CalendarWeekTimeGrid::DayColumnX(int dayIndex) const
{
	return CalendarWeekView::TIME_AXIS_WIDTH + dayIndex * DayWidth();
}

void CalendarWeekTimeGrid::SetStartOfWeek(const QDate &date)
{
	_startOfWeek = date;
	update();
}

void CalendarWeekTimeGrid::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	update();
}

void CalendarWeekTimeGrid::ScrollToCurrentTime(QScrollArea *sa)
{
	if (!sa) {
		return;
	}
	const int y = qMax(0, TimeToY(QTime::currentTime().addSecs(-3600)));
	sa->verticalScrollBar()->setValue(y);
}

// Returns the portion of an event visible within [date 00:00, date+1 00:00).
// drawStart / drawEnd are times in that day's coordinate space.
static bool ClipEventToDay(const CalendarEvent &ev, const QDate &date,
			   QTime &drawStart, QTime &drawEnd)
{
	if (!ev.start.isValid()) {
		return false;
	}
	const QDateTime dayStart(date, QTime(0, 0, 0));
	const QDateTime dayEnd(date.addDays(1), QTime(0, 0, 0));
	const QDateTime evEnd = ev.EffectiveEnd();

	if (ev.start >= dayEnd || evEnd <= dayStart) {
		return false; // no overlap
	}

	drawStart = (ev.start < dayStart) ? QTime(0, 0, 0) : ev.start.time();
	drawEnd = (evEnd >= dayEnd) ? QTime(23, 59, 59) : evEnd.time();
	return true;
}

QList<CalendarWeekTimeGrid::EventLayout>
CalendarWeekTimeGrid::LayoutDay(int dayIndex) const
{
	const QDate date = _startOfWeek.addDays(dayIndex);

	// Collect events that overlap this day, sorted by their clipped start.
	struct DayEvent {
		CalendarEvent ev;
		QTime drawStart;
		QTime drawEnd;
	};
	QList<DayEvent> dayEvents;
	for (const auto &ev : _events) {
		QTime ds, de;
		if (ClipEventToDay(ev, date, ds, de)) {
			dayEvents.append({ev, ds, de});
		}
	}

	std::sort(dayEvents.begin(), dayEvents.end(),
		  [](const DayEvent &a, const DayEvent &b) {
			  return a.drawStart < b.drawStart;
		  });

	QList<EventLayout> result;
	QList<QTime> columnEnds;

	for (const auto &de : dayEvents) {
		int col = -1;
		for (int i = 0; i < columnEnds.size(); ++i) {
			if (columnEnds[i] <= de.drawStart) {
				col = i;
				columnEnds[i] = de.drawEnd;
				break;
			}
		}
		if (col == -1) {
			col = columnEnds.size();
			columnEnds.append(de.drawEnd);
		}
		result.append({de.ev, de.drawStart, de.drawEnd, col, 0});
	}

	const int totalCols = qMax(1, (int)columnEnds.size());
	for (auto &layout : result) {
		layout.numCols = totalCols;
	}
	return result;
}

QString CalendarWeekTimeGrid::EventIdAtPoint(const QPoint &pos) const
{
	if (pos.x() < CalendarWeekView::TIME_AXIS_WIDTH) {
		return {};
	}
	const int dayW = DayWidth();
	const int dayIdx = (pos.x() - CalendarWeekView::TIME_AXIS_WIDTH) / dayW;
	if (dayIdx < 0 || dayIdx >= 7) {
		return {};
	}

	const int dayX = DayColumnX(dayIdx);
	for (const auto &layout : LayoutDay(dayIdx)) {
		const int colW = dayW / layout.numCols;
		const int x = dayX + layout.col * colW + 2;
		const int w = colW - 4;
		const int y = TimeToY(layout.drawStart);
		const int h = qMax(TimeToY(layout.drawEnd) - y, 20);
		if (QRect(x, y, w, h).contains(pos)) {
			return layout.event.id;
		}
	}
	return {};
}

QDateTime CalendarWeekTimeGrid::SlotAtPoint(const QPoint &pos) const
{
	if (pos.x() < CalendarWeekView::TIME_AXIS_WIDTH) {
		return {};
	}
	const int dayW = DayWidth();
	const int dayIdx = (pos.x() - CalendarWeekView::TIME_AXIS_WIDTH) / dayW;
	if (dayIdx < 0 || dayIdx >= 7) {
		return {};
	}
	return QDateTime(_startOfWeek.addDays(dayIdx), YToTime(pos.y()));
}

void CalendarWeekTimeGrid::DrawEvent(QPainter &p, const EventLayout &layout,
				     int dayX, int dayW)
{
	const auto &ev = layout.event;
	const int colW = dayW / layout.numCols;
	const int x = dayX + layout.col * colW + 2;
	const int w = colW - 4;
	const int y = TimeToY(layout.drawStart);
	const int endY = TimeToY(layout.drawEnd);
	const int h = qMax(endY - y, 20);
	const QRect rect(x, y, w, h);

	p.setBrush(ev.color);
	p.setPen(ev.color.darker(130));
	p.drawRoundedRect(rect, 3, 3);

	p.fillRect(x, y + 1, 3, h - 2, ev.color.darker(160));

	p.setPen(Qt::white);

	QFont f = p.font();
	f.setPixelSize(10);
	f.setBold(true);
	p.setFont(f);

	const QRect textRect = rect.adjusted(6, 2, -2, -2);
	if (h >= 34) {
		QFont tf = f;
		tf.setBold(false);
		p.setFont(tf);
		p.drawText(textRect, Qt::AlignTop | Qt::AlignLeft,
			   ev.start.toString("HH:mm"));
		p.setFont(f);
		p.drawText(textRect.adjusted(0, 14, 0, 0),
			   Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
			   ev.title);
	} else {
		p.drawText(textRect,
			   Qt::AlignVCenter | Qt::AlignLeft |
				   Qt::TextSingleLine,
			   ev.title);
	}
}

void CalendarWeekTimeGrid::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const int w = width();
	const int h = height();
	const int dayW = DayWidth();
	const QDate today = QDate::currentDate();

	p.fillRect(0, 0, w, h, palette().base());
	p.fillRect(0, 0, CalendarWeekView::TIME_AXIS_WIDTH, h,
		   palette().button());

	for (int d = 0; d < 7; ++d) {
		const QDate date = _startOfWeek.addDays(d);
		const int x = DayColumnX(d);
		if (date == today) {
			p.fillRect(x, 0, dayW, h,
				   palette().highlight().color().lighter(190));
		} else if (date.dayOfWeek() >= 6) {
			p.fillRect(x, 0, dayW, h,
				   palette().alternateBase().color());
		}
	}

	QFont labelFont = p.font();
	labelFont.setPixelSize(10);
	p.setFont(labelFont);

	for (int hour = 0; hour < 24; ++hour) {
		const int y = hour * CalendarWeekView::PIXELS_PER_HOUR;

		p.setPen(palette().text().color());
		p.drawText(2, y, CalendarWeekView::TIME_AXIS_WIDTH - 6,
			   CalendarWeekView::PIXELS_PER_HOUR,
			   Qt::AlignTop | Qt::AlignRight,
			   QTime(hour, 0).toString("HH:mm"));

		p.setPen(QPen(palette().mid().color(), 1));
		p.drawLine(CalendarWeekView::TIME_AXIS_WIDTH, y, w, y);

		QPen halfPen(palette().midlight().color(), 1, Qt::DotLine);
		p.setPen(halfPen);
		const int yHalf = y + CalendarWeekView::PIXELS_PER_HOUR / 2;
		p.drawLine(CalendarWeekView::TIME_AXIS_WIDTH, yHalf, w, yHalf);
	}

	p.setPen(QPen(palette().mid().color(), 1));
	for (int d = 0; d <= 7; ++d) {
		const int x = DayColumnX(d);
		p.drawLine(x, 0, x, h);
	}
	p.drawLine(CalendarWeekView::TIME_AXIS_WIDTH, 0,
		   CalendarWeekView::TIME_AXIS_WIDTH, h);

	for (int d = 0; d < 7; ++d) {
		const int dayX = DayColumnX(d);
		for (const auto &layout : LayoutDay(d)) {
			DrawEvent(p, layout, dayX, dayW);
		}
	}

	// Current-time indicator
	if (_startOfWeek.isValid() && _startOfWeek <= today &&
	    today <= _startOfWeek.addDays(6)) {
		const int dayIdx = _startOfWeek.daysTo(today);
		const int dayX = DayColumnX(dayIdx);
		const int y = TimeToY(QTime::currentTime());

		p.setPen(QPen(QColor(220, 30, 30), 2));
		p.drawLine(dayX, y, dayX + dayW, y);

		p.setBrush(QColor(220, 30, 30));
		p.setPen(Qt::NoPen);
		p.drawEllipse(dayX - 4, y - 4, 8, 8);
	}
}

void CalendarWeekTimeGrid::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const QString id = EventIdAtPoint(e->pos());
	if (!id.isEmpty()) {
		emit EventClicked(id);
		return;
	}
	const QDateTime slot = SlotAtPoint(e->pos());
	if (slot.isValid()) {
		emit SlotClicked(slot);
	}
}

void CalendarWeekTimeGrid::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton) {
		return;
	}
	const QString id = EventIdAtPoint(e->pos());
	if (!id.isEmpty()) {
		emit EventDoubleClicked(id);
	}
}

// ---------------------------------------------------------------------------
// CalendarWeekView
// ---------------------------------------------------------------------------

CalendarWeekView::CalendarWeekView(QWidget *parent) : CalendarView(parent)
{
	_header = new CalendarWeekDayHeader(this);
	_timeGrid = new CalendarWeekTimeGrid(this);

	// Fix the minimum height so the scroll area cannot shrink the grid
	// below the full 24-hour span — which would eliminate the scrollbar.
	_timeGrid->setMinimumHeight(24 * CalendarWeekView::PIXELS_PER_HOUR);

	_scrollArea = new QScrollArea(this);
	_scrollArea->setWidget(_timeGrid);
	// widgetResizable(true) lets the grid expand horizontally to fill the
	// viewport width while the fixed minimumHeight enforces vertical scrolling.
	_scrollArea->setWidgetResizable(true);
	_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	_scrollArea->setFrameShape(QFrame::NoFrame);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(_header);
	layout->addWidget(_scrollArea);
	setLayout(layout);

	connect(_timeGrid, &CalendarWeekTimeGrid::SlotClicked, this,
		&CalendarWeekView::SlotClicked);
	connect(_timeGrid, &CalendarWeekTimeGrid::EventClicked, this,
		&CalendarWeekView::EventClicked);
	connect(_timeGrid, &CalendarWeekTimeGrid::EventDoubleClicked, this,
		&CalendarWeekView::EventDoubleClicked);

	SetDate(QDate::currentDate());
}

void CalendarWeekView::SetDate(const QDate &date)
{
	_startOfWeek = date.addDays(-(date.dayOfWeek() - 1));
	UpdateViews();
	emit VisibleRangeChanged(RangeStart(), RangeEnd());
}

void CalendarWeekView::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	_timeGrid->SetEvents(events);
}

QDate CalendarWeekView::CurrentDate() const
{
	return _startOfWeek.addDays(3); // Wednesday - stable mid-point
}

QDate CalendarWeekView::RangeStart() const
{
	return _startOfWeek;
}

QDate CalendarWeekView::RangeEnd() const
{
	return _startOfWeek.addDays(6);
}

void CalendarWeekView::UpdateViews()
{
	_header->SetStartOfWeek(_startOfWeek);
	_timeGrid->SetStartOfWeek(_startOfWeek);
	_timeGrid->ScrollToCurrentTime(_scrollArea);
}

} // namespace advss

// Required for CalendarWeekTimeGrid defined in this file
#include "calendar-week-view.moc"
