#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto media_func = 6;
constexpr auto default_priority_6 = media_func;

struct MediaSwitch : SceneSwitcherEntry {
	OBSWeakSource source;
	obs_media_state state;
	time_restriction restriction;
	int64_t time;
	bool matched;
	std::string mediaSwitchStr;

	const char *getType() { return "meida"; }

	bool valid()
	{
		return (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(source) && WeakSourceValid(transition);
	}

	inline MediaSwitch(OBSWeakSource scene_, OBSWeakSource source_,
			   OBSWeakSource transition_, obs_media_state state_,
			   time_restriction restriction_, uint64_t time_,
			   bool usePreviousScene_, std::string mediaSwitchStr_)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  source(source_),
		  state(state_),
		  restriction(restriction_),
		  time(time_),
		  mediaSwitchStr(mediaSwitchStr_)
	{
	}
};
