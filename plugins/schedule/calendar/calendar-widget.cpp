#include "calendar-widget.hpp"

#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QHBoxLayout>
#include <QLocale>
#include <QVBoxLayout>

namespace advss {

CalendarWidget::CalendarWidget(QWidget *parent)
	: QWidget(parent),
	  _prevBtn(new QPushButton(this)),
	  _nextBtn(new QPushButton(this)),
	  _todayBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.calendar.today"), this)),
	  _navLabel(new QLabel(this)),
	  _monthBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.calendar.month"), this)),
	  _weekBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.calendar.week"), this)),
	  _dayBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.calendar.day"), this)),
	  _viewStack(new QStackedWidget(this)),
	  _monthView(new CalendarMonthView(this)),
	  _weekView(new CalendarWeekView(this)),
	  _dayView(new CalendarDayView(this))
{
	// --- Navigation bar ---
	SetButtonIcon(_prevBtn, GetThemeTypeName() == "Light"
					? "theme:Light/left.svg"
					: "theme:Dark/left.svg");
	SetButtonIcon(_nextBtn, GetThemeTypeName() == "Light"
					? "theme:Light/right.svg"
					: "theme:Dark/right.svg");
	_navLabel->setAlignment(Qt::AlignCenter);

	_monthBtn->setCheckable(true);
	_weekBtn->setCheckable(true);
	_dayBtn->setCheckable(true);
	_weekBtn->setChecked(true);

	auto navBar = new QHBoxLayout();
	navBar->addWidget(_prevBtn);
	navBar->addWidget(_nextBtn);
	navBar->addWidget(_todayBtn);
	navBar->addStretch();
	navBar->addWidget(_navLabel, 1);
	navBar->addStretch();
	navBar->addWidget(_monthBtn);
	navBar->addWidget(_weekBtn);
	navBar->addWidget(_dayBtn);

	// --- Views ---
	_viewStack->addWidget(_monthView);
	_viewStack->addWidget(_weekView);
	_viewStack->addWidget(_dayView);

	// --- Main layout ---
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->addLayout(navBar);
	mainLayout->addWidget(_viewStack, 1);
	setLayout(mainLayout);

	// --- Connections ---
	connect(_prevBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnPrevClicked);
	connect(_nextBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnNextClicked);
	connect(_todayBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnTodayClicked);
	connect(_monthBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnMonthModeClicked);
	connect(_weekBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnWeekModeClicked);
	connect(_dayBtn, &QPushButton::clicked, this,
		&CalendarWidget::OnDayModeClicked);

	ConnectView(_monthView);
	ConnectView(_weekView);
	ConnectView(_dayView);

	// Start in week view
	SwitchToView(_weekView);
	UpdateNavLabel();
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void CalendarWidget::SetEvents(const QList<CalendarEvent> &events)
{
	_events = events;
	_monthView->SetEvents(events);
	_weekView->SetEvents(events);
	_dayView->SetEvents(events);
}

void CalendarWidget::AddEvent(const CalendarEvent &event)
{
	_events.append(event);
	_monthView->SetEvents(_events);
	_weekView->SetEvents(_events);
	_dayView->SetEvents(_events);
}

void CalendarWidget::RemoveEvent(const QString &id)
{
	_events.erase(std::remove_if(_events.begin(), _events.end(),
				     [&id](const CalendarEvent &e) {
					     return e.id == id;
				     }),
		      _events.end());
	_monthView->SetEvents(_events);
	_weekView->SetEvents(_events);
	_dayView->SetEvents(_events);
}

void CalendarWidget::ClearEvents()
{
	_events.clear();
	_monthView->SetEvents(_events);
	_weekView->SetEvents(_events);
	_dayView->SetEvents(_events);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void CalendarWidget::GoToDate(const QDate &date)
{
	_activeView->SetDate(date);
	UpdateNavLabel();
}

void CalendarWidget::GoToToday()
{
	GoToDate(QDate::currentDate());
}

QDate CalendarWidget::VisibleRangeStart() const
{
	return _activeView ? _activeView->RangeStart() : QDate();
}

QDate CalendarWidget::VisibleRangeEnd() const
{
	return _activeView ? _activeView->RangeEnd() : QDate();
}

void CalendarWidget::OnPrevClicked()
{
	if (_viewMode == ViewMode::Month) {
		_activeView->SetDate(_activeView->CurrentDate().addMonths(-1));
	} else if (_viewMode == ViewMode::Week) {
		_activeView->SetDate(_activeView->RangeStart().addDays(-7));
	} else {
		_activeView->SetDate(_activeView->CurrentDate().addDays(-1));
	}
	UpdateNavLabel();
}

void CalendarWidget::OnNextClicked()
{
	if (_viewMode == ViewMode::Month) {
		_activeView->SetDate(_activeView->CurrentDate().addMonths(1));
	} else if (_viewMode == ViewMode::Week) {
		_activeView->SetDate(_activeView->RangeStart().addDays(7));
	} else {
		_activeView->SetDate(_activeView->CurrentDate().addDays(1));
	}
	UpdateNavLabel();
}

void CalendarWidget::OnTodayClicked()
{
	GoToToday();
}

void CalendarWidget::OnMonthModeClicked()
{
	SetViewMode(ViewMode::Month);
}

void CalendarWidget::OnWeekModeClicked()
{
	SetViewMode(ViewMode::Week);
}

void CalendarWidget::OnDayModeClicked()
{
	SetViewMode(ViewMode::Day);
}

void CalendarWidget::OnViewRangeChanged(const QDate &start, const QDate &end)
{
	emit VisibleRangeChanged(start, end);
	UpdateNavLabel();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void CalendarWidget::ConnectView(CalendarView *view)
{
	connect(view, &CalendarView::SlotClicked, this,
		&CalendarWidget::SlotClicked);
	connect(view, &CalendarView::EventClicked, this,
		&CalendarWidget::EventClicked);
	connect(view, &CalendarView::EventDoubleClicked, this,
		&CalendarWidget::EventDoubleClicked);
	connect(view, &CalendarView::VisibleRangeChanged, this,
		&CalendarWidget::OnViewRangeChanged);
}

void CalendarWidget::SwitchToView(CalendarView *view)
{
	_activeView = view;
	_viewStack->setCurrentWidget(view);
	view->SetEvents(_events);
	UpdateNavLabel();
}

void CalendarWidget::SetViewMode(ViewMode mode)
{
	if (_viewMode == mode) {
		return;
	}
	_viewMode = mode;

	// Preserve the current date when switching views
	const QDate current = _activeView->CurrentDate();

	_monthBtn->setChecked(mode == ViewMode::Month);
	_weekBtn->setChecked(mode == ViewMode::Week);
	_dayBtn->setChecked(mode == ViewMode::Day);

	switch (mode) {
	case ViewMode::Month:
		SwitchToView(_monthView);
		_monthView->SetDate(current);
		break;
	case ViewMode::Week:
		SwitchToView(_weekView);
		_weekView->SetDate(current);
		break;
	case ViewMode::Day: {
		SwitchToView(_dayView);
		const QDate today = QDate::currentDate();
		const QDate rangeStart = _activeView->RangeStart();
		const QDate rangeEnd = _activeView->RangeEnd();
		const QDate target = (today >= rangeStart && today <= rangeEnd)
					     ? today
					     : rangeStart;
		_dayView->SetDate(target);
		break;
	}
	}
	UpdateNavLabel();
}

void CalendarWidget::UpdateNavLabel()
{
	if (!_activeView) {
		return;
	}
	QLocale locale;
	const QDate cur = _activeView->CurrentDate();

	if (_viewMode == ViewMode::Month) {
		_navLabel->setText(locale.monthName(cur.month()) + "  " +
				   QString::number(cur.year()));
	} else if (_viewMode == ViewMode::Day) {
		// Day view: "Monday,  April 13,  2026"
		_navLabel->setText(locale.dayName(cur.dayOfWeek()) + ",  " +
				   locale.monthName(cur.month()) + "  " +
				   QString::number(cur.day()) + ",  " +
				   QString::number(cur.year()));
	} else {
		// Week view: "Apr 7 – Apr 13, 2026"
		const QDate s = _activeView->RangeStart();
		const QDate e = _activeView->RangeEnd();
		if (s.month() == e.month()) {
			_navLabel->setText(
				locale.monthName(s.month(),
						 QLocale::ShortFormat) +
				"  " + QString::number(s.day()) + " - " +
				QString::number(e.day()) + ",  " +
				QString::number(s.year()));
		} else {
			_navLabel->setText(
				locale.monthName(s.month(),
						 QLocale::ShortFormat) +
				" " + QString::number(s.day()) + " - " +
				locale.monthName(e.month(),
						 QLocale::ShortFormat) +
				" " + QString::number(e.day()) + ",  " +
				QString::number(s.year()));
		}
	}
}

} // namespace advss
