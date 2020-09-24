#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto round_trip_func = 1;
constexpr auto default_priority_1 = round_trip_func;

struct SceneRoundTripSwitch : SceneSwitcherEntry {
	OBSWeakSource startScene;
	double delay;
	std::string sceneRoundTripStr;

	const char *getType() { return "sequence"; }

	bool valid()
	{
		return WeakSourceValid(startScene) &&
		       (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(transition);
	}

	void logSleep(int dur)
	{
		blog(LOG_INFO, "Advanced Scene Switcher sequence sleep %d",
		     dur);
	}

	inline SceneRoundTripSwitch(OBSWeakSource startScene_,
				    OBSWeakSource scene_,
				    OBSWeakSource transition_, double delay_,
				    bool usePreviousScene_, std::string str)
		: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
		  startScene(startScene_),
		  delay(delay_),
		  sceneRoundTripStr(str)
	{
	}
};

static inline QString MakeSceneRoundTripSwitchName(const QString &scene1,
						   const QString &scene2,
						   const QString &transition,
						   double delay);
