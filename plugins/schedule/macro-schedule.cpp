#include "macro-schedule.hpp"

#include "log-helper.hpp"
#include "macro-export-extensions.hpp"
#include "macro-helpers.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"

#include <obs-data.h>
#include <QUuid>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace advss {

static std::deque<MacroScheduleEntry> scheduleEntries;

static std::thread schedulerThread;
static std::atomic<bool> schedulerRunning{false};
static std::mutex schedulerWaitMutex;
static std::condition_variable schedulerWaitCV;

static void initScheduler();
static void cleanupScheduler();
static void saveEntries(obs_data_t *obj);
static void loadEntries(obs_data_t *obj);
static bool setup();
static bool setupDone = setup();

static bool setup()
{
	AddSaveStep(saveEntries);
	AddLoadStep(loadEntries);
	AddStartStep(initScheduler);
	AddStopStep(cleanupScheduler);
	AddPluginCleanupStep([]() {
		cleanupScheduler();
		scheduleEntries.clear();
	});
	AddMacroExportExtension(
		{"AdvSceneSwitcher.macroTab.export.macroScheduleEntries",
		 "macroScheduleEntries",
		 [](obs_data_t *data, const QStringList &selectedIds) {
			 OBSDataArrayAutoRelease array =
				 obs_data_array_create();
			 auto lock = LockContext();
			 for (const auto &entry : scheduleEntries) {
				 if (!selectedIds.isEmpty() &&
				     !selectedIds.contains(
					     QString::fromStdString(entry.id)))
					 continue;
				 OBSDataAutoRelease item = obs_data_create();
				 entry.Save(item);
				 obs_data_array_push_back(array, item);
			 }
			 obs_data_set_array(data, "macroScheduleEntries",
					    array);
		 },
		 [](obs_data_t *data, const QStringList &) {
			 OBSDataArrayAutoRelease array = obs_data_get_array(
				 data, "macroScheduleEntries");
			 if (!array) {
				 return;
			 }
			 const size_t count = obs_data_array_count(array);
			 auto lock = LockContext();
			 for (size_t i = 0; i < count; ++i) {
				 OBSDataAutoRelease item =
					 obs_data_array_item(array, i);
				 MacroScheduleEntry entry;
				 entry.Load(item);
				 // Skip if an entry with this id already exists.
				 const auto &id = entry.id;
				 const bool exists = std::any_of(
					 scheduleEntries.begin(),
					 scheduleEntries.end(),
					 [&id](const MacroScheduleEntry &e) {
						 return e.id == id;
					 });
				 if (exists) {
					 continue;
				 }
				 // Reset runtime state so the entry fires fresh.
				 entry.timesTriggered = 0;
				 entry.lastTriggered = QDateTime();
				 entry.endDateActionApplied = false;
				 scheduleEntries.emplace_back(std::move(entry));
			 }
		 },
		 []() -> QList<QPair<QString, QString>> {
			 QList<QPair<QString, QString>> items;
			 auto lock = LockContext();
			 for (const auto &entry : scheduleEntries) {
				 items.append({QString::fromStdString(entry.id),
					       entry.GetSummary()});
			 }
			 return items;
		 }});
	return true;
}

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------

MacroScheduleEntry::MacroScheduleEntry()
	: id(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()),
	  startDateTime(QDateTime::currentDateTime())
{
}

static void saveDateTime(obs_data_t *obj, const char *key, const QDateTime &dt)
{
	if (dt.isValid()) {
		obs_data_set_string(
			obj, key,
			dt.toString(Qt::ISODate).toUtf8().constData());
	}
}

static QDateTime loadDateTime(obs_data_t *obj, const char *key)
{
	const char *str = obs_data_get_string(obj, key);
	if (!str || !*str) {
		return QDateTime();
	}
	return QDateTime::fromString(QString::fromUtf8(str), Qt::ISODate);
}

void MacroScheduleEntry::Save(obs_data_t *obj) const
{
	obs_data_set_string(obj, "id", id.c_str());
	obs_data_set_string(obj, "name", name.c_str());
	macro.Save(obj);
	obs_data_set_string(obj, "color", color.name().toUtf8().constData());
	obs_data_set_bool(obj, "checkConditions", checkConditions);
	obs_data_set_bool(obj, "runElseActionsOnConditionFailure",
			  runElseActionsOnConditionFailure);
	obs_data_set_bool(obj, "enabled", enabled);

	saveDateTime(obj, "startDateTime", startDateTime);
	obs_data_set_bool(obj, "hasEndDate", hasEndDate);
	if (hasEndDate) {
		saveDateTime(obj, "endDate", endDate);
		obs_data_set_int(obj, "endDateAction",
				 static_cast<int>(endDateAction));
	}

	obs_data_set_int(obj, "repeatType", static_cast<int>(repeatType));
	obs_data_set_int(obj, "repeatInterval", repeatInterval);
	obs_data_set_int(obj, "repeatEndType", static_cast<int>(repeatEndType));
	obs_data_set_int(obj, "repeatMaxCount", repeatMaxCount);
	if (repeatEndType == RepeatEndType::UNTIL_DATE &&
	    repeatUntilDate.isValid()) {
		saveDateTime(obj, "repeatUntilDate", repeatUntilDate);
	}

	obs_data_set_int(obj, "timesTriggered", timesTriggered);
	saveDateTime(obj, "lastTriggered", lastTriggered);
	obs_data_set_bool(obj, "endDateActionApplied", endDateActionApplied);
}

void MacroScheduleEntry::Load(obs_data_t *obj)
{
	const char *idStr = obs_data_get_string(obj, "id");
	if (idStr && *idStr) {
		id = idStr;
	}
	const char *nameStr = obs_data_get_string(obj, "name");
	if (nameStr) {
		name = nameStr;
	}
	macro.Load(obj);
	const char *colorStr = obs_data_get_string(obj, "color");
	if (colorStr && *colorStr) {
		const QColor loaded(QString::fromUtf8(colorStr));
		if (loaded.isValid()) {
			color = loaded;
		}
	}
	checkConditions = obs_data_get_bool(obj, "checkConditions");
	runElseActionsOnConditionFailure =
		obs_data_get_bool(obj, "runElseActionsOnConditionFailure");
	obs_data_set_default_bool(obj, "enabled", true);
	enabled = obs_data_get_bool(obj, "enabled");

	startDateTime = loadDateTime(obj, "startDateTime");
	hasEndDate = obs_data_get_bool(obj, "hasEndDate");
	if (hasEndDate) {
		endDate = loadDateTime(obj, "endDate");
		endDateAction = static_cast<EndDateAction>(
			obs_data_get_int(obj, "endDateAction"));
	}

	repeatType =
		static_cast<RepeatType>(obs_data_get_int(obj, "repeatType"));
	obs_data_set_default_int(obj, "repeatInterval", 1);
	repeatInterval = (int)obs_data_get_int(obj, "repeatInterval");
	repeatEndType = static_cast<RepeatEndType>(
		obs_data_get_int(obj, "repeatEndType"));
	obs_data_set_default_int(obj, "repeatMaxCount", 1);
	repeatMaxCount = (int)obs_data_get_int(obj, "repeatMaxCount");
	repeatUntilDate = loadDateTime(obj, "repeatUntilDate");

	timesTriggered = (int)obs_data_get_int(obj, "timesTriggered");
	lastTriggered = loadDateTime(obj, "lastTriggered");
	endDateActionApplied = obs_data_get_bool(obj, "endDateActionApplied");
}

QDateTime MacroScheduleEntry::advanceFrom(const QDateTime &base) const
{
	switch (repeatType) {
	case RepeatType::MINUTELY:
		return base.addSecs((qint64)repeatInterval * 60);
	case RepeatType::HOURLY:
		return base.addSecs((qint64)repeatInterval * 3600);
	case RepeatType::DAILY:
		return base.addDays(repeatInterval);
	case RepeatType::WEEKLY:
		return base.addDays((qint64)repeatInterval * 7);
	case RepeatType::MONTHLY:
		return base.addMonths(repeatInterval);
	default:
		return QDateTime();
	}
}

QDateTime MacroScheduleEntry::NextTriggerTime() const
{
	if (!lastTriggered.isValid()) {
		return startDateTime;
	}
	if (repeatType == RepeatType::NONE) {
		// If the start was moved beyond the last trigger, treat as not yet fired.
		if (startDateTime > lastTriggered) {
			return startDateTime;
		}
		return QDateTime(); // one-shot already triggered
	}

	QDateTime next = startDateTime;
	while (next <= lastTriggered) {
		QDateTime candidate = advanceFrom(next);
		if (!candidate.isValid()) {
			return QDateTime();
		}
		next = candidate;
	}
	return next;
}

bool MacroScheduleEntry::IsExpired() const
{
	if (repeatType == RepeatType::NONE && lastTriggered.isValid()) {
		return lastTriggered >= startDateTime;
	}
	switch (repeatEndType) {
	case RepeatEndType::NEVER:
		return false;
	case RepeatEndType::AFTER_N_TIMES:
		return timesTriggered >= repeatMaxCount;
	case RepeatEndType::UNTIL_DATE:
		return repeatUntilDate.isValid() &&
		       QDateTime::currentDateTime() > repeatUntilDate;
	}
	return false;
}

bool MacroScheduleEntry::ShouldTrigger(const QDateTime &now) const
{
	if (!enabled) {
		return false;
	}
	if (IsExpired()) {
		return false;
	}
	if (!startDateTime.isValid()) {
		return false;
	}
	const QDateTime next = NextTriggerTime();
	if (!next.isValid()) {
		return false;
	}
	return next <= now;
}

void MacroScheduleEntry::MarkTriggered(const QDateTime &now)
{
	++timesTriggered;
	lastTriggered = now;
}

void MacroScheduleEntry::FastForwardTo(const QDateTime &now)
{
	timesTriggered = 0;
	lastTriggered = QDateTime();

	if (!startDateTime.isValid()) {
		return;
	}

	if (repeatType == RepeatType::NONE) {
		// One-shot: count it as triggered if the start lies in the past.
		if (startDateTime <= now) {
			timesTriggered = 1;
			lastTriggered = startDateTime;
		}
		return;
	}

	// Repeating: walk intervals from startDateTime and count each one
	// that falls at or before 'now'.
	QDateTime t = startDateTime;
	while (t <= now) {
		++timesTriggered;
		lastTriggered = t;
		const QDateTime next = advanceFrom(t);
		if (!next.isValid()) {
			break;
		}
		t = next;
	}
}

QString MacroScheduleEntry::GetRepeatDescription() const
{
	if (repeatType == RepeatType::NONE) {
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.once");
	}
	const char *unitSingular = "";
	const char *unitPlural = "";
	switch (repeatType) {
	case RepeatType::MINUTELY:
		unitSingular = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.minute");
		unitPlural = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.minutes");
		break;
	case RepeatType::HOURLY:
		unitSingular = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.hour");
		unitPlural = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.hours");
		break;
	case RepeatType::DAILY:
		unitSingular = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.day");
		unitPlural = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.days");
		break;
	case RepeatType::WEEKLY:
		unitSingular = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.week");
		unitPlural = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.weeks");
		break;
	case RepeatType::MONTHLY:
		unitSingular = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.month");
		unitPlural = obs_module_text(
			"AdvSceneSwitcher.macroScheduleEntry.repeat.unit.months");
		break;
	default:
		break;
	}
	if (repeatInterval == 1) {
		return QString(obs_module_text(
				       "AdvSceneSwitcher.macroScheduleEntry.repeat.everyOne"))
			.arg(unitSingular);
	}
	return QString(obs_module_text(
			       "AdvSceneSwitcher.macroScheduleEntry.repeat.everyN"))
		.arg(repeatInterval)
		.arg(unitPlural);
}

QString MacroScheduleEntry::GetSummary() const
{
	if (!name.empty()) {
		return QString::fromStdString(name);
	}
	const std::string macroName = macro.Name();
	return macroName.empty()
		       ? QString(obs_module_text(
				 "AdvSceneSwitcher.macroScheduleEntry.noMacro"))
		       : QString::fromStdString(macroName);
}

QString MacroScheduleEntry::GetNextTriggerString() const
{
	if (!enabled) {
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleTab.status.disabled");
	}
	if (IsExpired()) {
		return obs_module_text(
			"AdvSceneSwitcher.macroScheduleTab.status.expired");
	}
	const QDateTime next = NextTriggerTime();
	if (!next.isValid()) {
		return QString("-");
	}
	return next.toString("yyyy-MM-dd HH:mm");
}

std::deque<MacroScheduleEntry> &GetScheduleEntries()
{
	return scheduleEntries;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

static void saveEntries(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &entry : scheduleEntries) {
		OBSDataAutoRelease item = obs_data_create();
		entry.Save(item);
		obs_data_array_push_back(array, item);
	}
	obs_data_set_array(obj, "macroScheduleEntries", array);
}

static void loadEntries(obs_data_t *obj)
{
	scheduleEntries.clear();
	OBSDataArrayAutoRelease array =
		obs_data_get_array(obj, "macroScheduleEntries");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; ++i) {
		OBSDataAutoRelease item = obs_data_array_item(array, i);
		scheduleEntries.emplace_back();
		scheduleEntries.back().Load(item);
	}
}

// ---------------------------------------------------------------------------
// Scheduler thread
// ---------------------------------------------------------------------------

static void applyEndDateAction(MacroScheduleEntry &entry)
{
	if (!entry.hasEndDate || !entry.endDate.isValid()) {
		return;
	}
	if (entry.endDateAction == MacroScheduleEntry::EndDateAction::NONE) {
		return;
	}

	switch (entry.endDateAction) {
	case MacroScheduleEntry::EndDateAction::DISABLE_ENTRY:
		entry.enabled = false;
		break;
	case MacroScheduleEntry::EndDateAction::PAUSE_MACRO:
		StopMacro(entry.macro.GetMacro().get());
		SetMacroPaused(entry.macro.GetMacro().get(), true);
		break;
	case MacroScheduleEntry::EndDateAction::STOP_MACRO:
		StopMacro(entry.macro.GetMacro().get());
		break;
	default:
		break;
	}
}

static void checkAndFireEntries()
{
	const QDateTime now = QDateTime::currentDateTime();
	auto lock = LockContext();

	for (auto &entry : scheduleEntries) {
		// Apply end-date action once when the entry transitions to expired.
		// We detect the transition by checking whether the end date has
		// just passed while the entry is still nominally enabled.
		if (entry.hasEndDate && entry.endDate.isValid() &&
		    entry.enabled && now >= entry.endDate &&
		    entry.endDateAction !=
			    MacroScheduleEntry::EndDateAction::NONE &&
		    !entry.endDateActionApplied) {
			applyEndDateAction(entry);
			entry.endDateActionApplied = true;
		}

		if (!entry.ShouldTrigger(now)) {
			continue;
		}

		auto macro = entry.macro.GetMacro();
		if (!macro) {
			// Advance state so we don't spam-check a missing macro
			entry.MarkTriggered(now);
			blog(LOG_WARNING,
			     "[macro-schedule] Scheduled macro '%s' not found, skipping.",
			     entry.macro.Name().c_str());
			continue;
		}

		if (entry.checkConditions) {
			if (CheckMacroConditions(macro.get(), true)) {
				RunMacroActions(macro.get(), true, true);
			} else if (entry.runElseActionsOnConditionFailure) {
				RunMacroElseActions(macro.get(), true, true);
			}
		} else {
			RunMacroActions(macro.get(), true, true);
		}
		entry.MarkTriggered(now);
	}
}

static void initScheduler()
{
	if (schedulerRunning.exchange(true)) {
		return; // already running
	}
	schedulerThread = std::thread([]() {
		while (schedulerRunning) {
			checkAndFireEntries();
			std::unique_lock<std::mutex> lock(schedulerWaitMutex);
			schedulerWaitCV.wait_for(
				lock, std::chrono::seconds(1),
				[]() { return !schedulerRunning.load(); });
		}
	});
}

static void cleanupScheduler()
{
	schedulerRunning = false;
	schedulerWaitCV.notify_all();
	if (schedulerThread.joinable()) {
		schedulerThread.join();
	}
}

} // namespace advss
