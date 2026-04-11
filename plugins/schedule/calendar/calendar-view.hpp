#pragma once
#include "calendar-event.hpp"

#include <QDate>
#include <QList>
#include <QWidget>

namespace advss {

// Abstract base class for calendar view modes (month, week, …).
// Subclasses must implement the pure virtual interface and emit the
// signals defined here via Q_SIGNALS so that CalendarWidget can
// connect to them uniformly regardless of the active view.
class CalendarView : public QWidget {
	Q_OBJECT

public:
	explicit CalendarView(QWidget *parent = nullptr) : QWidget(parent) {}

	// Navigate to show the given date. The visible range is determined
	// by the concrete view (e.g. the whole month, or the week).
	virtual void SetDate(const QDate &date) = 0;

	// Replace the full set of events shown in this view.
	virtual void SetEvents(const QList<CalendarEvent> &events) = 0;

	// A representative date for the current position (used for navigation).
	virtual QDate CurrentDate() const = 0;

	// Inclusive range of dates currently rendered.
	virtual QDate RangeStart() const = 0;
	virtual QDate RangeEnd() const = 0;

signals:
	// User clicked an empty time slot.
	void SlotClicked(const QDateTime &startTime);

	// User single-clicked an event (id = CalendarEvent::id).
	void EventClicked(const QString &eventId);

	// User double-clicked an event.
	void EventDoubleClicked(const QString &eventId);

	// The visible date range changed; the owner should refresh events.
	void VisibleRangeChanged(const QDate &rangeStart,
				 const QDate &rangeEnd);
};

} // namespace advss
