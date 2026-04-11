#pragma once
#include "calendar-day-view.hpp"
#include "calendar-event.hpp"
#include "calendar-month-view.hpp"
#include "calendar-week-view.hpp"

#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

namespace advss {

class CalendarWidget : public QWidget {
	Q_OBJECT

public:
	enum class ViewMode { Month, Week, Day };

	explicit CalendarWidget(QWidget *parent = nullptr);

	// --- View ---
	void SetViewMode(ViewMode mode);
	ViewMode GetViewMode() const { return _viewMode; }

	// --- Events ---
	void SetEvents(const QList<CalendarEvent> &events);
	void AddEvent(const CalendarEvent &event);
	void RemoveEvent(const QString &id);
	void ClearEvents();
	const QList<CalendarEvent> &GetEvents() const { return _events; }

	// --- Navigation ---
	void GoToDate(const QDate &date);
	void GoToToday();

	// --- Visible range ---
	QDate VisibleRangeStart() const;
	QDate VisibleRangeEnd() const;

signals:
	// User clicked an empty slot in the active view.
	void SlotClicked(const QDateTime &startTime);

	// User single-clicked an event.
	void EventClicked(const QString &eventId);

	// User double-clicked an event (typically: open edit dialog).
	void EventDoubleClicked(const QString &eventId);

	// The visible date range changed; reload your events for [start, end].
	void VisibleRangeChanged(const QDate &rangeStart,
				 const QDate &rangeEnd);

private slots:
	void OnPrevClicked();
	void OnNextClicked();
	void OnTodayClicked();
	void OnMonthModeClicked();
	void OnWeekModeClicked();
	void OnDayModeClicked();
	void OnViewRangeChanged(const QDate &start, const QDate &end);

private:
	void ConnectView(CalendarView *view);
	void SwitchToView(CalendarView *view);
	void UpdateNavLabel();

	// Navigation bar widgets
	QPushButton *_prevBtn;
	QPushButton *_nextBtn;
	QPushButton *_todayBtn;
	QLabel *_navLabel;
	QPushButton *_monthBtn;
	QPushButton *_weekBtn;
	QPushButton *_dayBtn;

	// View stack
	QStackedWidget *_viewStack;
	CalendarMonthView *_monthView;
	CalendarWeekView *_weekView;
	CalendarDayView *_dayView;
	CalendarView *_activeView = nullptr;

	ViewMode _viewMode = ViewMode::Week;
	QList<CalendarEvent> _events;
};

} // namespace advss
