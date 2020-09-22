#pragma once
#include <string>
#include <obs.hpp>
#include "utility.hpp"

constexpr auto media_func = 6;
constexpr auto default_priority_6 = media_func;

struct MediaSwitch {
	OBSWeakSource scene;
	OBSWeakSource source;
	OBSWeakSource transition;
	obs_media_state state;
	int64_t time;
	time_restriction restriction;
	bool matched;
	bool usePreviousScene;
	std::string mediaSwitchStr;

	bool valid()
	{
		return (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(source) && WeakSourceValid(transition);
	}

	inline MediaSwitch(OBSWeakSource scene_, OBSWeakSource source_,
			   OBSWeakSource transition_, obs_media_state state_,
			   time_restriction restriction_, uint64_t time_,
			   bool usePreviousScene_, std::string mediaSwitchStr_)
		: scene(scene_),
		  source(source_),
		  transition(transition_),
		  state(state_),
		  restriction(restriction_),
		  time(time_),
		  usePreviousScene(usePreviousScene_),
		  mediaSwitchStr(mediaSwitchStr_)
	{
	}
};
