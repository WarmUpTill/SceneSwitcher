#pragma once
#include <string>
#include <obs.hpp>

constexpr auto default_idle_time = 60;
constexpr auto idle_func = 2;
constexpr auto default_priority_2 = idle_func;

struct IdleData {
	bool idleEnable = false;
	int time = default_idle_time;
	OBSWeakSource scene;
	OBSWeakSource transition;
	bool usePreviousScene;
	bool alreadySwitched = false;
};
