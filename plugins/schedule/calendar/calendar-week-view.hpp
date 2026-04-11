#pragma once
#include "calendar-view.hpp"

#include <QScrollArea>

namespace advss {

class CalendarWeekDayHeader;
class CalendarWeekTimeGrid;

// Displays 7 day columns with a vertical time axis (00:00 - 24:00).
// Events are rendered as colored blocks sized to their duration.
// Overlapping events within the same day are arranged in side-by-side
// sub-columns. A red indicator marks the current time.
class CalendarWeekView : public CalendarView {
	Q_OBJECT

public:
	explicit CalendarWeekView(QWidget *parent = nullptr);

	void SetDate(const QDate &date) override;
	void SetEvents(const QList<CalendarEvent> &events) override;

	QDate CurrentDate() const override;
	QDate RangeStart() const override;
	QDate RangeEnd() const override;

	// Shared layout constants (used by DayHeader and TimeGrid)
	static constexpr int TIME_AXIS_WIDTH = 56;
	static constexpr int PIXELS_PER_HOUR = 64;
	static constexpr int DAY_HEADER_HEIGHT = 36;
	static constexpr int MIN_DAY_WIDTH = 80;

private:
	void UpdateViews();

	CalendarWeekDayHeader *_header;
	CalendarWeekTimeGrid *_timeGrid;
	QScrollArea *_scrollArea;

	QDate _startOfWeek;
	QList<CalendarEvent> _events;
};

} // namespace advss
