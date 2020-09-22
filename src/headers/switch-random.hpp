#pragma once
#include <string>
#include "utility.hpp"

struct RandomSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	double delay;
	std::string randomSwitchStr;

	bool valid()
	{
		return WeakSourceValid(scene) && WeakSourceValid(transition);
	}

	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			    double delay_, std::string str)
		: scene(scene_),
		  transition(transition_),
		  delay(delay_),
		  randomSwitchStr(str)
	{
	}
};
