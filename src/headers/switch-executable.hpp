#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

constexpr auto exe_func = 3;
constexpr auto default_priority_3 = exe_func;

struct ExecutableSceneSwitch : SceneSwitcherEntry {
	QString exe;
	bool inFocus;

	const char *getType() { return "exec"; }

	inline ExecutableSceneSwitch(OBSWeakSource scene_,
				     OBSWeakSource transition_,
				     const QString &exe_, bool inFocus_)
		: SceneSwitcherEntry(scene_, transition_),
		  exe(exe_),
		  inFocus(inFocus_)
	{
	}
};

static inline QString MakeSwitchNameExecutable(const QString &scene,
					       const QString &value,
					       const QString &transition,
					       bool inFocus);
