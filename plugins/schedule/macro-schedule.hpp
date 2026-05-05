#pragma once
#include "macro-ref.hpp"

#include <QColor>
#include <QDateTime>
#include <QString>
#include <deque>

namespace advss {

class MacroScheduleEntry {
public:
	enum class RepeatType {
		NONE,
		MINUTELY,
		HOURLY,
		DAILY,
		WEEKLY,
		MONTHLY,
	};

	enum class RepeatEndType {
		NEVER,
		AFTER_N_TIMES,
		UNTIL_DATE,
	};

	enum class EndDateAction {
		NONE,
		DISABLE_ENTRY,
		PAUSE_MACRO,
		STOP_MACRO,
	};

	MacroScheduleEntry();

	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	QDateTime NextTriggerTime() const;
	bool ShouldTrigger(const QDateTime &now) const;
	void MarkTriggered(const QDateTime &now);
	bool IsExpired() const;
	void FastForwardTo(const QDateTime &now);

	QString GetSummary() const;
	QString GetRepeatDescription() const;
	QString GetNextTriggerString() const;

	std::string id;
	std::string name;
	MacroRef macro;
	QColor color{70, 130, 180};
	bool checkConditions = false;
	bool runElseActionsOnConditionFailure = false;
	bool enabled = true;

	QDateTime startDateTime;
	bool hasEndDate = false;
	QDateTime endDate;
	EndDateAction endDateAction = EndDateAction::NONE;

	RepeatType repeatType = RepeatType::NONE;
	int repeatInterval = 1;

	RepeatEndType repeatEndType = RepeatEndType::NEVER;
	int repeatMaxCount = 1;
	QDateTime repeatUntilDate;

	// Persisted runtime state
	int timesTriggered = 0;
	QDateTime lastTriggered;
	bool endDateActionApplied = false;

private:
	QDateTime advanceFrom(const QDateTime &base) const;
};

std::deque<MacroScheduleEntry> &GetScheduleEntries();

} // namespace advss
