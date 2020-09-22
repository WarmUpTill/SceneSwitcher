#pragma once
#include <string>
#include <QDateTime>
#include <obs.hpp>

constexpr auto time_func = 7;
constexpr auto default_priority_7 = time_func;

struct TimeSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	timeTrigger trigger;
	QTime time;
	bool usePreviousScene;
	std::string timeSwitchStr;

	inline TimeSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			  timeTrigger trigger_, QTime time_,
			  bool usePreviousScene_, std::string timeSwitchStr_)
		: scene(scene_),
		  transition(transition_),
		  trigger(trigger_),
		  time(time_),
		  usePreviousScene(usePreviousScene_),
		  timeSwitchStr(timeSwitchStr_)
	{
	}
};
