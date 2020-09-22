#pragma once
#include <string>
#include <obs.hpp>

constexpr auto round_trip_func = 1;
constexpr auto default_priority_1 = round_trip_func;

struct SceneRoundTripSwitch {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	double delay;
	bool usePreviousScene;
	std::string sceneRoundTripStr;

	inline SceneRoundTripSwitch(OBSWeakSource scene1_,
				    OBSWeakSource scene2_,
				    OBSWeakSource transition_, double delay_,
				    bool usePreviousScene_, std::string str)
		: scene1(scene1_),
		  scene2(scene2_),
		  transition(transition_),
		  delay(delay_),
		  usePreviousScene(usePreviousScene_),
		  sceneRoundTripStr(str)
	{
	}
};
