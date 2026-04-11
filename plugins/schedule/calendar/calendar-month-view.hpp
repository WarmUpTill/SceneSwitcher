#pragma once
#include "calendar-view.hpp"

namespace advss {

// Displays a traditional month grid (6 weeks × 7 days).
// Events are rendered as small colored bars inside each day cell.
// Up to three events are shown per cell; additional events are
// indicated by a "+N more" label.
class CalendarMonthView : public CalendarView {
	Q_OBJECT

public:
	explicit CalendarMonthView(QWidget *parent = nullptr);

	void SetDate(const QDate &date) override;
	void SetEvents(const QList<CalendarEvent> &events) override;

	QDate CurrentDate() const override { return _currentDate; }
	QDate RangeStart() const override { return _gridStart; }
	QDate RangeEnd() const override { return _gridStart.addDays(41); }

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;

private:
	// Geometry helpers
	QDate CellDate(int row, int col) const;
	QRect CellRect(int row, int col) const;
	bool CellFromPoint(const QPoint &pos, int &row, int &col) const;

	// Event helpers
	QList<CalendarEvent> EventsForDate(const QDate &date) const;
	QString EventIdAtPoint(const QPoint &pos) const;

	// Painting
	void PaintDayNameHeader(QPainter &p);
	void PaintCell(QPainter &p, int row, int col);

	static constexpr int ROWS = 6;
	static constexpr int COLS = 7;
	static constexpr int HEADER_HEIGHT = 28; // day-name row
	static constexpr int DAY_NUM_HEIGHT =
		22; // space reserved for day number
	static constexpr int EVENT_HEIGHT = 16;
	static constexpr int EVENT_MARGIN = 2;
	static constexpr int CELL_PAD = 3;
	static constexpr int MAX_VISIBLE_EVENTS = 3;

	QDate _currentDate;
	QDate _gridStart; // Monday of the week that contains the 1st of the month
	QList<CalendarEvent> _events;
};

} // namespace advss
