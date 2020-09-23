#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto screen_region_func = 4;
constexpr auto default_priority_4 = screen_region_func;

struct ScreenRegionSwitch : SceneSwitcherEntry {
	int minX, minY, maxX, maxY;
	std::string regionStr;

	const char *getType() { return "region"; }

	inline ScreenRegionSwitch(OBSWeakSource scene_,
				  OBSWeakSource transition_, int minX_,
				  int minY_, int maxX_, int maxY_,
				  std::string regionStr_)
		: SceneSwitcherEntry(scene_, transition_),
		  minX(minX_),
		  minY(minY_),
		  maxX(maxX_),
		  maxY(maxY_),
		  regionStr(regionStr_)
	{
	}
};
