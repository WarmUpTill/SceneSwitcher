#pragma once
#include <string>
#include <obs.hpp>

struct RandomSwitch {
	OBSWeakSource scene;
	OBSWeakSource transition;
	double delay;
	std::string randomSwitchStr;

	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			    double delay_, std::string str)
		: scene(scene_),
		  transition(transition_),
		  delay(delay_),
		  randomSwitchStr(str)
	{
	}
};
