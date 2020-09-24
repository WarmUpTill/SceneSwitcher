#pragma once
#include <string>
#include <QDateTime>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto time_func = 7;
constexpr auto default_priority_7 = time_func;

typedef enum {
	ANY_DAY = 0,
	MONDAY = 1,
	TUSEDAY = 2,
	WEDNESDAY = 3,
	THURSDAY = 4,
	FRIDAY = 5,
	SATURDAY = 6,
	SUNDAY = 7,
	LIVE = 8
} timeTrigger;

struct TimeSwitch : SceneSwitcherEntry {
	timeTrigger trigger;
	QTime time;
	std::string timeSwitchStr;

	const char *getType() { return "time"; }

	inline TimeSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  timeTrigger trigger_, QTime time_,
			  bool usePreviousScene_, std::string timeSwitchStr_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  trigger(trigger_),
		  time(time_),
		  timeSwitchStr(timeSwitchStr_)
	{
	}
};

static inline QString MakeTimeSwitchName(const QString &scene,
					 const QString &transition,
					 const timeTrigger &trigger,
					 const QTime &time);
