#pragma once
#include <string>
#include <obs.hpp>

constexpr auto window_title_func = 5;
constexpr auto default_priority_5 = window_title_func;

struct WindowSceneSwitch {
	OBSWeakSource scene;
	std::string window;
	OBSWeakSource transition;
	bool fullscreen;
	bool focus;

	inline WindowSceneSwitch(OBSWeakSource scene_, const char *window_,
				 OBSWeakSource transition_, bool fullscreen_,
				 bool focus_)
		: scene(scene_),
		  window(window_),
		  transition(transition_),
		  fullscreen(fullscreen_),
		  focus(focus_)
	{
	}
};
