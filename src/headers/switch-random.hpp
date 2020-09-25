#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

struct RandomSwitch : SceneSwitcherEntry {
	double delay;
	std::string randomSwitchStr;

	const char *getType() { return "random"; }

	inline RandomSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			    double delay_, std::string str)
		: SceneSwitcherEntry(scene_, transition_),
		  delay(delay_),
		  randomSwitchStr(str)
	{
	}
};

static inline QString MakeRandomSwitchName(const QString &scene,
					   const QString &transition,
					   double &delay);
