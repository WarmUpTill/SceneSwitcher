#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto window_title_func = 5;
constexpr auto default_priority_5 = window_title_func;

struct WindowSceneSwitch : SceneSwitcherEntry {
	std::string window;
	bool fullscreen;
	bool focus;

	const char *getType() { return "window"; }

	inline WindowSceneSwitch(OBSWeakSource scene_, const char *window_,
				 OBSWeakSource transition_, bool fullscreen_,
				 bool focus_)
		: SceneSwitcherEntry(scene_, transition_),
		  window(window_),
		  fullscreen(fullscreen_),
		  focus(focus_)
	{
	}
};

static inline QString MakeWindowSwitchName(const QString &scene,
					   const QString &value,
					   const QString &transition,
					   bool fullscreen, bool focus);
