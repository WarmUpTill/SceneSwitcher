#include "macro-schedule-tab.hpp"
#include "calendar/calendar-event.hpp"
#include "macro-schedule-entry-dialog.hpp"

#include "macro-helpers.hpp"
#include "macro-signals.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"

#include <QKeyEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QVBoxLayout>

namespace advss {

// ---------------------------------------------------------------------------
// Tab registration
// ---------------------------------------------------------------------------

static bool registerTab();
static bool registerTabDone = registerTab();

static MacroScheduleTab *tabWidget = nullptr;

static constexpr int MACRO_COUNT_THRESHOLD = 5;

static void setTabVisible(QTabWidget *tab, bool visible)
{
	SetTabVisibleByName(
		tab, visible,
		obs_module_text("AdvSceneSwitcher.macroScheduleTab.title"));
}

static bool enoughMacros()
{
	return (int)GetAllMacros().size() >= MACRO_COUNT_THRESHOLD;
}

static void setupTab(QTabWidget *tab)
{
	if (!GetScheduleEntries().empty()) {
		setTabVisible(tab, true);
		return;
	}

	if (!enoughMacros()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(MacroSignalManager::Instance(),
			 &MacroSignalManager::Add, tab, [tab](const QString &) {
				 if (enoughMacros()) {
					 setTabVisible(tab, true);
				 }
			 });
}

static bool registerTab()
{
	AddSetupTabCallback("macroScheduleTab", MacroScheduleTab::Create,
			    setupTab);
	return true;
}

// ---------------------------------------------------------------------------
// Entry -> CalendarEvent conversion
// ---------------------------------------------------------------------------

static QColor ColorForEntry(const MacroScheduleEntry &entry)
{
	if (!entry.enabled || entry.IsExpired()) {
		return QColor(150, 150, 150);
	}
	return entry.color.isValid() ? entry.color : QColor(70, 130, 180);
}

// Returns one CalendarEvent per occurrence of entry within [rangeStart, rangeEnd].
// For non-repeating entries this is at most one event.
// A hard cap of 500 occurrences prevents infinite loops for misconfigured entries.
static QList<CalendarEvent>
EntryToCalendarEvents(const MacroScheduleEntry &entry, const QDate &rangeStart,
		      const QDate &rangeEnd)
{
	QList<CalendarEvent> result;

	const QDateTime windowStart(rangeStart, QTime(0, 0));
	const QDateTime windowEnd(rangeEnd, QTime(23, 59, 59));

	// Walk occurrences starting from the entry's first trigger time.
	// We use a copy to advance lastTriggered without modifying the real entry.
	MacroScheduleEntry cursor = entry;
	cursor.lastTriggered =
		QDateTime(); // start from the very first occurrence
	cursor.timesTriggered = 0;

	static constexpr int MAX_OCCURRENCES = 500;

	for (int i = 0; i < MAX_OCCURRENCES; ++i) {
		const QDateTime next = cursor.NextTriggerTime();
		if (!next.isValid() || next > windowEnd) {
			break;
		}

		if (next >= windowStart) {
			CalendarEvent ev;
			// Use id + occurrence index to make each event unique
			// while still allowing the tab to look up the parent entry.
			ev.id = QString::fromStdString(entry.id);
			ev.title = entry.GetSummary();
			ev.color = ColorForEntry(entry);
			ev.userData = ev.id;
			ev.start = next;
			if (entry.hasEndDate && entry.endDate.isValid()) {
				ev.end = entry.endDate;
			}
			result.append(ev);
		}

		if (entry.repeatType == MacroScheduleEntry::RepeatType::NONE) {
			break; // one-shot — no more occurrences
		}

		// Advance cursor past this occurrence
		cursor.lastTriggered = next;
		++cursor.timesTriggered;

		if (cursor.IsExpired()) {
			break;
		}
	}

	return result;
}

// ---------------------------------------------------------------------------
// Table columns
// ---------------------------------------------------------------------------

static constexpr int COL_NAME = 0;
static constexpr int COL_MACRO = 1;
static constexpr int COL_SCHEDULE = 2;
static constexpr int COL_NEXT_TRIGGER = 3;
static constexpr int COL_STATUS = 4;
static constexpr int COL_COUNT = 5;

// ---------------------------------------------------------------------------
// MacroScheduleTab
// ---------------------------------------------------------------------------

MacroScheduleTab *MacroScheduleTab::Create()
{
	tabWidget = new MacroScheduleTab();
	return tabWidget;
}

MacroScheduleTab::MacroScheduleTab(QWidget *parent)
	: QWidget(parent),
	  _calendar(new CalendarWidget(this)),
	  _table(new QTableWidget(0, COL_COUNT, this)),
	  _helpLabel(new QLabel(
		  obs_module_text("AdvSceneSwitcher.macroScheduleTab.help"),
		  this)),
	  _addBtn(new QToolButton(this)),
	  _editBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.macroScheduleTab.edit"),
		  this)),
	  _removeBtn(new QToolButton(this)),
	  _toggleBtn(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.macroScheduleTab.enable"),
		  this)),
	  _refreshTimer(new QTimer(this))
{
	// --- Add / Remove: icon buttons (same style as ResourceTable) ---
	_addBtn->setProperty("themeID",
			     QVariant(QString::fromUtf8("addIconSmall")));
	_addBtn->setProperty("class", QVariant(QString::fromUtf8("icon-plus")));
	_addBtn->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroScheduleTab.add.tooltip"));

	_removeBtn->setProperty("themeID",
				QVariant(QString::fromUtf8("removeIconSmall")));
	_removeBtn->setProperty("class",
				QVariant(QString::fromUtf8("icon-trash")));
	_removeBtn->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroScheduleTab.remove.tooltip"));

	if (GetScheduleEntries().empty()) {
		_addButtonHighlight =
			HighlightWidget(_addBtn, QColor(Qt::green));
	}

	// --- Table ---
	_table->setSelectionBehavior(QAbstractItemView::SelectRows);
	_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
	_table->setContextMenuPolicy(Qt::CustomContextMenu);
	_table->installEventFilter(this);
	_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_table->setAlternatingRowColors(true);
	_table->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeToContents);
	_table->horizontalHeader()->setStretchLastSection(true);
	_table->verticalHeader()->setVisible(false);
	_table->setHorizontalHeaderLabels(
		{obs_module_text(
			 "AdvSceneSwitcher.macroScheduleTab.column.name"),
		 obs_module_text(
			 "AdvSceneSwitcher.macroScheduleTab.column.macro"),
		 obs_module_text(
			 "AdvSceneSwitcher.macroScheduleTab.column.schedule"),
		 obs_module_text(
			 "AdvSceneSwitcher.macroScheduleTab.column.nextTrigger"),
		 obs_module_text(
			 "AdvSceneSwitcher.macroScheduleTab.column.status")});

	// --- Help label ---
	_helpLabel->setAlignment(Qt::AlignCenter);
	_helpLabel->setWordWrap(true);

	// --- Button row below the table ---
	auto buttonRow = new QHBoxLayout();
	buttonRow->setContentsMargins(0, 0, 0, 0);
	buttonRow->addWidget(_addBtn);
	buttonRow->addWidget(_removeBtn);
	buttonRow->addStretch();
	buttonRow->addWidget(_editBtn);
	buttonRow->addWidget(_toggleBtn);

	// --- Right panel: help + table + buttons ---
	auto rightPanel = new QWidget(this);
	auto rightLayout = new QVBoxLayout(rightPanel);
	rightLayout->setContentsMargins(0, 0, 0, 0);
	rightLayout->addWidget(_helpLabel);
	rightLayout->addWidget(_table, 1);
	rightLayout->addLayout(buttonRow);
	rightPanel->setLayout(rightLayout);

	// --- Splitter ---
	auto splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(_calendar);
	splitter->addWidget(rightPanel);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 1);

	// --- Main layout ---
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);
	mainLayout->addWidget(splitter, 1);
	setLayout(mainLayout);

	// --- Connections: toolbar buttons ---
	connect(_addBtn, &QPushButton::clicked, this, &MacroScheduleTab::Add);
	connect(_editBtn, &QPushButton::clicked, this, &MacroScheduleTab::Edit);
	connect(_removeBtn, &QPushButton::clicked, this,
		&MacroScheduleTab::Remove);
	connect(_toggleBtn, &QPushButton::clicked, this,
		&MacroScheduleTab::ToggleEnabled);
	connect(_table, &QWidget::customContextMenuRequested, this,
		&MacroScheduleTab::ShowContextMenu);

	// --- Connections: table ---
	connect(_table, &QTableWidget::doubleClicked, this,
		[this]() { Edit(); });
	connect(_table->selectionModel(),
		&QItemSelectionModel::selectionChanged, this,
		&MacroScheduleTab::OnSelectionChanged);

	// --- Connections: calendar ---
	connect(_calendar, &CalendarWidget::SlotClicked, this,
		&MacroScheduleTab::AddAtTime);
	connect(_calendar, &CalendarWidget::EventClicked, this,
		&MacroScheduleTab::SelectTableRowById);
	connect(_calendar, &CalendarWidget::EventDoubleClicked, this,
		&MacroScheduleTab::EditById);
	connect(_calendar, &CalendarWidget::VisibleRangeChanged, this,
		[this](const QDate &, const QDate &) {
			std::deque<MacroScheduleEntry> snapshot;
			{
				auto lock = LockContext();
				snapshot = GetScheduleEntries();
			}
			RefreshCalendar(snapshot);
		});

	// --- Periodic refresh ---
	_refreshTimer->setInterval(30000); // 30 seconds
	connect(_refreshTimer, &QTimer::timeout, this,
		&MacroScheduleTab::Refresh);
	_refreshTimer->start();

	// Defer the initial refresh: Create() is called while the context lock
	// is already held, so we cannot call LockContext() here directly.
	QTimer::singleShot(0, this, &MacroScheduleTab::Refresh);
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void MacroScheduleTab::Refresh()
{
	std::deque<MacroScheduleEntry> snapshot;
	{
		auto lock = LockContext();
		snapshot = GetScheduleEntries();
	}

	PopulateTable();
	RefreshCalendar(snapshot);
	UpdateButtonStates();
}

void MacroScheduleTab::RefreshCalendar(
	const std::deque<MacroScheduleEntry> &entries)
{
	const QDate rangeStart = _calendar->VisibleRangeStart();
	const QDate rangeEnd = _calendar->VisibleRangeEnd();

	QList<CalendarEvent> events;
	for (const auto &entry : entries) {
		events.append(
			EntryToCalendarEvents(entry, rangeStart, rangeEnd));
	}
	_calendar->SetEvents(events);
}

void MacroScheduleTab::PopulateTable()
{
	// Remember which entry was selected so we can restore it after repopulating
	const int prevIdx = SelectedEntryIndex();
	QString prevId;
	std::deque<MacroScheduleEntry> snapshot;
	{
		auto lock = LockContext();
		const auto &entries = GetScheduleEntries();
		if (prevIdx >= 0 && prevIdx < (int)entries.size()) {
			prevId = QString::fromStdString(entries[prevIdx].id);
		}
		snapshot = entries;
	}

	_table->setRowCount(0);

	for (const auto &entry : snapshot) {
		const int row = _table->rowCount();
		_table->insertRow(row);

		auto makeItem = [](const QString &text) {
			auto item = new QTableWidgetItem(text);
			item->setFlags(item->flags() & ~Qt::ItemIsEditable);
			return item;
		};

		_table->setItem(row, COL_NAME, makeItem(entry.GetSummary()));
		_table->setItem(
			row, COL_MACRO,
			makeItem(QString::fromStdString(entry.macro.Name())));
		_table->setItem(row, COL_SCHEDULE,
				makeItem(entry.GetRepeatDescription()));
		_table->setItem(row, COL_NEXT_TRIGGER,
				makeItem(entry.GetNextTriggerString()));

		QString status;
		if (!entry.enabled) {
			status = obs_module_text(
				"AdvSceneSwitcher.macroScheduleTab.status.disabled");
		} else if (entry.IsExpired()) {
			status = obs_module_text(
				"AdvSceneSwitcher.macroScheduleTab.status.expired");
		} else {
			status = obs_module_text(
				"AdvSceneSwitcher.macroScheduleTab.status.active");
		}
		_table->setItem(row, COL_STATUS, makeItem(status));

		_table->item(row, COL_NAME)
			->setData(Qt::UserRole,
				  QString::fromStdString(entry.id));
	}

	SetHelpVisible(_table->rowCount() == 0);

	// Restore selection
	if (!prevId.isEmpty()) {
		SelectTableRowById(prevId);
	}
}

void MacroScheduleTab::SetHelpVisible(bool visible)
{
	_helpLabel->setVisible(visible);
	_table->setVisible(!visible);
}

// ---------------------------------------------------------------------------
// Selection helpers
// ---------------------------------------------------------------------------

int MacroScheduleTab::SelectedEntryIndex() const
{
	const auto selected = _table->selectionModel()->selectedRows();
	if (selected.isEmpty()) {
		return -1;
	}
	const QString id = _table->item(selected.first().row(), COL_NAME)
				   ->data(Qt::UserRole)
				   .toString();
	const auto &entries = GetScheduleEntries();
	for (int i = 0; i < (int)entries.size(); ++i) {
		if (QString::fromStdString(entries[i].id) == id) {
			return i;
		}
	}
	return -1;
}

void MacroScheduleTab::SelectTableRowById(const QString &entryId)
{
	for (int row = 0; row < _table->rowCount(); ++row) {
		auto item = _table->item(row, COL_NAME);
		if (item && item->data(Qt::UserRole).toString() == entryId) {
			_table->selectRow(row);
			_table->scrollToItem(item);
			return;
		}
	}
}

QStringList MacroScheduleTab::SelectedEntryIds() const
{
	QStringList ids;
	for (const auto &index : _table->selectionModel()->selectedRows()) {
		auto item = _table->item(index.row(), COL_NAME);
		if (item) {
			ids.append(item->data(Qt::UserRole).toString());
		}
	}
	return ids;
}

bool MacroScheduleTab::eventFilter(QObject *obj, QEvent *event)
{
	if (obj == _table && event->type() == QEvent::KeyPress) {
		const auto keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Delete) {
			Remove();
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void MacroScheduleTab::UpdateButtonStates()
{
	const QStringList ids = SelectedEntryIds();
	const bool hasSelection = !ids.isEmpty();
	const bool singleSelection = ids.size() == 1;
	_editBtn->setEnabled(singleSelection);
	_removeBtn->setEnabled(hasSelection);
	_toggleBtn->setEnabled(hasSelection);

	if (hasSelection) {
		// Label reflects whether the action will enable or disable:
		// disable if all selected are enabled, otherwise enable.
		bool allEnabled = true;
		{
			auto lock = LockContext();
			for (const auto &entry : GetScheduleEntries()) {
				if (ids.contains(
					    QString::fromStdString(entry.id)) &&
				    !entry.enabled) {
					allEnabled = false;
					break;
				}
			}
		}
		_toggleBtn->setText(obs_module_text(
			allEnabled
				? "AdvSceneSwitcher.macroScheduleTab.disable"
				: "AdvSceneSwitcher.macroScheduleTab.enable"));
	}
}

void MacroScheduleTab::OnSelectionChanged()
{
	UpdateButtonStates();

	// Highlight the corresponding calendar event
	const int idx = SelectedEntryIndex();
	if (idx >= 0) {
		QDate triggerDate;
		{
			auto lock = LockContext();
			const auto &entries = GetScheduleEntries();
			if (idx < (int)entries.size()) {
				const QDateTime next =
					entries[idx].NextTriggerTime();
				if (next.isValid()) {
					triggerDate = next.date();
				}
			}
		}
		if (triggerDate.isValid()) {
			_calendar->GoToDate(triggerDate);
		}
	}
}

// ---------------------------------------------------------------------------
// CRUD actions
// ---------------------------------------------------------------------------

void MacroScheduleTab::Add()
{
	AddAtTime(QDateTime::currentDateTime());
}

void MacroScheduleTab::AddAtTime(const QDateTime &startTime)
{
	MacroScheduleEntry entry;
	entry.startDateTime = startTime;

	if (!MacroScheduleEntryDialog::AskForSettings(this, entry, true)) {
		return;
	}
	const QString newId = QString::fromStdString(entry.id);
	{
		auto lock = LockContext();
		GetScheduleEntries().emplace_back(std::move(entry));
	}
	Refresh();
	SelectTableRowById(newId);

	if (_addButtonHighlight) {
		_addButtonHighlight->deleteLater();
		_addButtonHighlight = nullptr;
	}
}

void MacroScheduleTab::Edit()
{
	const int idx = SelectedEntryIndex();
	if (idx < 0) {
		return;
	}
	MacroScheduleEntry copy;
	{
		auto lock = LockContext();
		copy = GetScheduleEntries()[idx];
	}
	const QString id = QString::fromStdString(copy.id);
	if (!MacroScheduleEntryDialog::AskForSettings(this, copy, false)) {
		return;
	}

	{
		auto lock = LockContext();
		GetScheduleEntries()[idx] = std::move(copy);
	}
	Refresh();
	SelectTableRowById(id);
}

void MacroScheduleTab::EditById(const QString &entryId)
{
	SelectTableRowById(entryId);
	Edit();
}

void MacroScheduleTab::Remove()
{
	const QStringList ids = SelectedEntryIds();
	if (ids.isEmpty()) {
		return;
	}

	// Build a confirmation message
	QString msg;
	if (ids.size() == 1) {
		QString name;
		{
			auto lock = LockContext();
			const int idx = SelectedEntryIndex();
			if (idx >= 0) {
				name = GetScheduleEntries()[idx].GetSummary();
			}
		}
		msg = QString(obs_module_text(
				      "AdvSceneSwitcher.macroScheduleTab.remove.confirm"))
			      .arg(name);
	} else {
		msg = QString(obs_module_text(
				      "AdvSceneSwitcher.macroScheduleTab.remove.confirmMultiple"))
			      .arg(ids.size());
	}

	if (!DisplayMessage(msg, true)) {
		return;
	}

	{
		auto lock = LockContext();
		auto &entries = GetScheduleEntries();
		entries.erase(
			std::remove_if(entries.begin(), entries.end(),
				       [&ids](const MacroScheduleEntry &e) {
					       return ids.contains(
						       QString::fromStdString(
							       e.id));
				       }),
			entries.end());
	}
	Refresh();
}

void MacroScheduleTab::ToggleEnabled()
{
	const QStringList ids = SelectedEntryIds();
	if (ids.isEmpty()) {
		return;
	}

	// If all selected entries are enabled, disable them; otherwise enable all.
	{
		auto lock = LockContext();
		bool allEnabled = true;
		for (const auto &entry : GetScheduleEntries()) {
			if (ids.contains(QString::fromStdString(entry.id)) &&
			    !entry.enabled) {
				allEnabled = false;
				break;
			}
		}
		const bool newState = !allEnabled;
		for (auto &entry : GetScheduleEntries()) {
			if (ids.contains(QString::fromStdString(entry.id))) {
				entry.enabled = newState;
			}
		}
	}
	Refresh();
}

// ---------------------------------------------------------------------------
// Context menu
// ---------------------------------------------------------------------------

void MacroScheduleTab::ShowContextMenu(const QPoint &pos)
{
	const QStringList ids = SelectedEntryIds();
	const bool hasSelection = !ids.isEmpty();
	const bool singleSelection = ids.size() == 1;

	QMenu menu(this);

	auto addAction = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroScheduleTab.add"), this,
		&MacroScheduleTab::Add);
	(void)addAction;

	menu.addSeparator();

	auto editAction = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroScheduleTab.edit"), this,
		&MacroScheduleTab::Edit);
	editAction->setEnabled(singleSelection);

	auto removeAction = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroScheduleTab.remove"),
		this, &MacroScheduleTab::Remove);
	removeAction->setEnabled(hasSelection);

	bool allEnabled = true;
	if (hasSelection) {
		auto lock = LockContext();
		for (const auto &entry : GetScheduleEntries()) {
			if (ids.contains(QString::fromStdString(entry.id)) &&
			    !entry.enabled) {
				allEnabled = false;
				break;
			}
		}
	}
	auto toggleAction = menu.addAction(
		obs_module_text(
			allEnabled
				? "AdvSceneSwitcher.macroScheduleTab.disable"
				: "AdvSceneSwitcher.macroScheduleTab.enable"),
		this, &MacroScheduleTab::ToggleEnabled);
	toggleAction->setEnabled(hasSelection);

	menu.exec(_table->viewport()->mapToGlobal(pos));
}

} // namespace advss
