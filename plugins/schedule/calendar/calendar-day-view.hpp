#pragma once
#include "calendar-view.hpp"

#include <QScrollArea>

namespace advss {

class CalendarDayHeader;
class CalendarDayTimeGrid;

// Displays a single day column with a vertical time axis (00:00 - 24:00).
// Events are rendered as colored blocks sized to their duration.
// Overlapping events are arranged in side-by-side sub-columns.
// A red indicator marks the current time.
class CalendarDayView : public CalendarView {
	Q_OBJECT

public:
	explicit CalendarDayView(QWidget *parent = nullptr);

	void SetDate(const QDate &date) override;
	void SetEvents(const QList<CalendarEvent> &events) override;

	QDate CurrentDate() const override;
	QDate RangeStart() const override;
	QDate RangeEnd() const override;

	// Shared layout constants (used by DayHeader and TimeGrid)
	static constexpr int TIME_AXIS_WIDTH = 56;
	static constexpr int PIXELS_PER_HOUR = 64;
	static constexpr int DAY_HEADER_HEIGHT = 36;

private:
	void UpdateViews();

	CalendarDayHeader *_header;
	CalendarDayTimeGrid *_timeGrid;
	QScrollArea *_scrollArea;

	QDate _date;
	QList<CalendarEvent> _events;
};

} // namespace advss
