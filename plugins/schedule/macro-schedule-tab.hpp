#pragma once

#include "calendar/calendar-widget.hpp"
#include "macro-schedule.hpp"

#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QWidget>

class QTabWidget;

namespace advss {

// The main tab widget registered with the settings window.
class MacroScheduleTab : public QWidget {
	Q_OBJECT

public:
	static MacroScheduleTab *Create();

	// Reload all data into the table and calendar
	void Refresh();

private slots:
	void Add();
	void AddAtTime(const QDateTime &startTime);
	void Edit();
	void EditById(const QString &entryId);
	void Remove();
	void ToggleEnabled();
	void ShowContextMenu(const QPoint &pos);
	void OnSelectionChanged();

private:
	explicit MacroScheduleTab(QWidget *parent = nullptr);

	bool eventFilter(QObject *obj, QEvent *event) override;

	void PopulateTable();
	void RefreshCalendar(const std::deque<MacroScheduleEntry> &entries);
	void SelectTableRowById(const QString &entryId);
	void UpdateButtonStates();
	void SetHelpVisible(bool visible);
	// Returns the index in GetScheduleEntries() for the first selected row,
	// or -1 if nothing is selected.
	int SelectedEntryIndex() const;
	// Returns the IDs of all selected rows.
	QStringList SelectedEntryIds() const;

	CalendarWidget *_calendar;
	QTableWidget *_table;
	QLabel *_helpLabel;

	QToolButton *_addBtn;
	QPushButton *_editBtn;
	QToolButton *_removeBtn;
	QPushButton *_toggleBtn;

	// Periodic refresh to update "Next Trigger" column
	QTimer *_refreshTimer;

	QObject *_addButtonHighlight = nullptr;
};

} // namespace advss
