#pragma once
#include <string>
#include <QDateTime>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto time_func = 7;
constexpr auto default_priority_7 = time_func;

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
