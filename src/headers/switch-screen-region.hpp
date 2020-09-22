#pragma once
#include <string>
#include <obs.hpp>

constexpr auto screen_region_func = 4;
constexpr auto default_priority_4 = screen_region_func;

struct ScreenRegionSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	int minX, minY, maxX, maxY;
	std::string regionStr;

	bool valid()
	{
		return WeakSourceValid(scene) && WeakSourceValid(transition);
	}

	inline ScreenRegionSwitch(OBSWeakSource scene_,
				  OBSWeakSource transition_, int minX_,
				  int minY_, int maxX_, int maxY_,
				  std::string regionStr_)
		: scene(scene_),
		  transition(transition_),
		  minX(minX_),
		  minY(minY_),
		  maxX(maxX_),
		  maxY(maxY_),
		  regionStr(regionStr_)
	{
	}
};
