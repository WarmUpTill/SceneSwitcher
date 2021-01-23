#pragma once
#include "switch-generic.hpp"

constexpr auto default_idle_time = 60;
constexpr auto idle_func = 2;
constexpr auto default_priority_2 = idle_func;

struct IdleData : SceneSwitcherEntry {
	static bool pause;
	bool idleEnable = false;
	int time = default_idle_time;
	bool alreadySwitched = false;

	const char *getType() { return "idle"; }
	void save(obs_data_t *obj);
	void load(obs_data_t *obj);
};
