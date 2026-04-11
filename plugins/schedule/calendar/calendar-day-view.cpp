#include "calendar-day-view.hpp"

#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <algorithm>

namespace advss {

// ---------------------------------------------------------------------------
// CalendarDayHeader – fixed date strip above the scroll area
// ---------------------------------------------------------------------------

class CalendarDayHeader : public QWidget {
public:
	explicit CalendarDayHeader(QWidget *parent = nullptr);
	void SetDate(const QDate &date);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	QDate _date;
};

CalendarDayHeader::CalendarDayHeader(QWidget *parent) : QWidget(parent)
{
	setFixedHeight(CalendarDayView::DAY_HEADER_HEIGHT);
}

void CalendarDayHeader::SetDate(const QDate &date)
{
	_date = date;
	update();
}

void CalendarDayHeader::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const QDate today = QDate::currentDate();
	const bool isToday = (_date == today);
	QLocale locale;

	// Time-axis placeholder
	p.fillRect(0, 0, CalendarDayView::TIME_AXIS_WIDTH, height(),
		   palette().button());

	// Day column
	const int colX = CalendarDayView::TIME_AXIS_WIDTH;
	const int colW = width() - colX;

	QColor bg = palette().button().color();
	if (isToday) {
		bg = palette().highlight().color().lighter(165);
	}
	p.fillRect(colX, 0, colW, height(), bg);

	p.setPen(palette().mid().color());
	p.drawLine(colX, 0, colX, height());

	QFont f = p.font();
	f.setPixelSize(13);
	f.setBold(isToday);
	p.setFont(f);
	p.setPen(isToday ? palette().highlight().color()
			 : palette().buttonText().color());

	const QString label =
		locale.dayName(_date.dayOfWeek(), QLocale::LongFormat) + "  " +
		QString::number(_date.day()) + "  " +
		locale.monthName(_date.month(), QLocale::ShortFormat) + "  " +
		QString::number(_date.year());
	p.drawText(colX, 0, colW, height(), Qt::AlignCenter, label);

	p.setPen(palette().mid().color());
	p.drawLine(0, height() - 1, width(), height() - 1);
}

// ---------------------------------------------------------------------------
// CalendarDayTimeGrid – scrollable single-day painted time grid
// ---------------------------------------------------------------------------

class CalendarDayTimeGrid : public QWidget {
	Q_OBJECT

public:
	explicit CalendarDayTimeGrid(QWidget *parent = nullptr);
	void SetDate(const QDate &date);
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
	int ColWidth() const;

	QList<EventLayout> LayoutDay() const;
	QString EventIdAtPoint(const QPoint &pos) const;
	QDateTime SlotAtPoint(const QPoint &pos) const;

	void DrawEvent(QPainter &p, const EventLayout &layout, int colW);

	QDate _date;
	QList<CalendarEvent> _events;
	int _refreshTimerId = 0;
};

CalendarDayTimeGrid::CalendarDayTimeGrid(QWidget *parent) : QWidget(parent)
{
	_refreshTimerId = startTimer(60 * 1000);
}

QSize CalendarDayTimeGrid::sizeHint() const
{
	return QSize(CalendarDayView::TIME_AXIS_WIDTH + 200,
		     24 * CalendarDayView::PIXELS_PER_HOUR);
}

void CalendarDayTimeGrid::timerEvent(QTimerEvent *e)
{
	if (e->timerId() == _refreshTimerId) {
		update();
	}
}

int CalendarDayTimeGrid::TimeToY(const QTime &t) const
{
	return (t.hour() * 60 + t.minute()) * CalendarDayView::PIXELS_PER_HOUR /
	       60;
}

QTime CalendarDayTimeGrid::YToTime(int y) const
{
	int totalMin = y * 60 / CalendarDayView::PIXELS_PER_HOUR;
	totalMin = qBound(0, totalMin, 24 * 60 - 1);
	totalMin = (totalMin / 15) * 15; // snap to 15-min increments
	return QTime(totalMin / 60, totalMin % 60);
}

int CalendarDayTimeGrid::ColWidth() const
{
	return qMax(0, width() - CalendarDayView::TIME_AXIS_WIDTH);
}

void CalendarDayTimeGrid::SetDate(const QDate &date)
{
	_date = date;
	update();
}

void CalendarDayTimeGrid::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	update();
}

void CalendarDayTimeGrid::ScrollToCurrentTime(QScrollArea *sa)
{
	if (!sa) {
		return;
	}
	const int y = qMax(0, TimeToY(QTime::currentTime().addSecs(-3600)));
	sa->verticalScrollBar()->setValue(y);
}

static bool ClipEventToDayD(const CalendarEvent &ev, const QDate &date,
			    QTime &drawStart, QTime &drawEnd)
{
	if (!ev.start.isValid()) {
		return false;
	}
	const QDateTime dayStart(date, QTime(0, 0, 0));
	const QDateTime dayEnd(date.addDays(1), QTime(0, 0, 0));
	const QDateTime evEnd = ev.EffectiveEnd();

	if (ev.start >= dayEnd || evEnd <= dayStart) {
		return false;
	}

	drawStart = (ev.start < dayStart) ? QTime(0, 0, 0) : ev.start.time();
	drawEnd = (evEnd >= dayEnd) ? QTime(23, 59, 59) : evEnd.time();
	return true;
}

QList<CalendarDayTimeGrid::EventLayout> CalendarDayTimeGrid::LayoutDay() const
{
	struct DayEvent {
		CalendarEvent ev;
		QTime drawStart;
		QTime drawEnd;
	};
	QList<DayEvent> dayEvents;
	for (const auto &ev : _events) {
		QTime ds, de;
		if (ClipEventToDayD(ev, _date, ds, de)) {
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

QString CalendarDayTimeGrid::EventIdAtPoint(const QPoint &pos) const
{
	if (pos.x() < CalendarDayView::TIME_AXIS_WIDTH) {
		return {};
	}
	const int colW = ColWidth();
	for (const auto &layout : LayoutDay()) {
		const int subW = colW / layout.numCols;
		const int x = CalendarDayView::TIME_AXIS_WIDTH +
			      layout.col * subW + 2;
		const int w = subW - 4;
		const int y = TimeToY(layout.drawStart);
		const int h = qMax(TimeToY(layout.drawEnd) - y, 20);
		if (QRect(x, y, w, h).contains(pos)) {
			return layout.event.id;
		}
	}
	return {};
}

QDateTime CalendarDayTimeGrid::SlotAtPoint(const QPoint &pos) const
{
	if (pos.x() < CalendarDayView::TIME_AXIS_WIDTH) {
		return {};
	}
	return QDateTime(_date, YToTime(pos.y()));
}

void CalendarDayTimeGrid::DrawEvent(QPainter &p, const EventLayout &layout,
				    int colW)
{
	const auto &ev = layout.event;
	const int subW = colW / layout.numCols;
	const int x = CalendarDayView::TIME_AXIS_WIDTH + layout.col * subW + 2;
	const int w = subW - 4;
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

void CalendarDayTimeGrid::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const int w = width();
	const int h = height();
	const int colW = ColWidth();
	const QDate today = QDate::currentDate();

	p.fillRect(0, 0, w, h, palette().base());
	p.fillRect(0, 0, CalendarDayView::TIME_AXIS_WIDTH, h,
		   palette().button());

	// Day column background
	const int colX = CalendarDayView::TIME_AXIS_WIDTH;
	if (_date == today) {
		p.fillRect(colX, 0, colW, h,
			   palette().highlight().color().lighter(190));
	} else if (_date.dayOfWeek() >= 6) {
		p.fillRect(colX, 0, colW, h, palette().alternateBase().color());
	}

	QFont labelFont = p.font();
	labelFont.setPixelSize(10);
	p.setFont(labelFont);

	for (int hour = 0; hour < 24; ++hour) {
		const int y = hour * CalendarDayView::PIXELS_PER_HOUR;

		p.setPen(palette().text().color());
		p.drawText(2, y, CalendarDayView::TIME_AXIS_WIDTH - 6,
			   CalendarDayView::PIXELS_PER_HOUR,
			   Qt::AlignTop | Qt::AlignRight,
			   QTime(hour, 0).toString("HH:mm"));

		p.setPen(QPen(palette().mid().color(), 1));
		p.drawLine(CalendarDayView::TIME_AXIS_WIDTH, y, w, y);

		QPen halfPen(palette().midlight().color(), 1, Qt::DotLine);
		p.setPen(halfPen);
		const int yHalf = y + CalendarDayView::PIXELS_PER_HOUR / 2;
		p.drawLine(CalendarDayView::TIME_AXIS_WIDTH, yHalf, w, yHalf);
	}

	p.setPen(QPen(palette().mid().color(), 1));
	p.drawLine(colX, 0, colX, h);

	for (const auto &layout : LayoutDay()) {
		DrawEvent(p, layout, colW);
	}

	// Current-time indicator
	if (_date == today) {
		const int y = TimeToY(QTime::currentTime());
		p.setPen(QPen(QColor(220, 30, 30), 2));
		p.drawLine(colX, y, w, y);
		p.setBrush(QColor(220, 30, 30));
		p.setPen(Qt::NoPen);
		p.drawEllipse(colX - 4, y - 4, 8, 8);
	}
}

void CalendarDayTimeGrid::mousePressEvent(QMouseEvent *e)
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

void CalendarDayTimeGrid::mouseDoubleClickEvent(QMouseEvent *e)
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
// CalendarDayView
// ---------------------------------------------------------------------------

CalendarDayView::CalendarDayView(QWidget *parent)
	: CalendarView(parent),
	  _header(new CalendarDayHeader(this)),
	  _timeGrid(new CalendarDayTimeGrid(this)),
	  _scrollArea(new QScrollArea(this))
{
	_timeGrid->setMinimumHeight(24 * CalendarDayView::PIXELS_PER_HOUR);

	_scrollArea->setWidget(_timeGrid);
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

	connect(_timeGrid, &CalendarDayTimeGrid::SlotClicked, this,
		&CalendarDayView::SlotClicked);
	connect(_timeGrid, &CalendarDayTimeGrid::EventClicked, this,
		&CalendarDayView::EventClicked);
	connect(_timeGrid, &CalendarDayTimeGrid::EventDoubleClicked, this,
		&CalendarDayView::EventDoubleClicked);

	SetDate(QDate::currentDate());
}

void CalendarDayView::SetDate(const QDate &date)
{
	_date = date;
	UpdateViews();
	emit VisibleRangeChanged(RangeStart(), RangeEnd());
}

void CalendarDayView::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	_timeGrid->SetEvents(events);
}

QDate CalendarDayView::CurrentDate() const
{
	return _date;
}

QDate CalendarDayView::RangeStart() const
{
	return _date;
}

QDate CalendarDayView::RangeEnd() const
{
	return _date;
}

void CalendarDayView::UpdateViews()
{
	_header->SetDate(_date);
	_timeGrid->SetDate(_date);
	_timeGrid->ScrollToCurrentTime(_scrollArea);
}

} // namespace advss

// Required for CalendarDayTimeGrid defined in this file
#include "calendar-day-view.moc"
