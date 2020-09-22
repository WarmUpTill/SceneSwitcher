#pragma once
#include <string>
#include <obs.hpp>

constexpr auto exe_func = 3;
constexpr auto default_priority_3 = exe_func;

struct ExecutableSceneSwitch {
	OBSWeakSource mScene;
	OBSWeakSource mTransition;
	QString mExe;
	bool mInFocus;

	bool valid()
	{
		return WeakSourceValid(mScene) && WeakSourceValid(mTransition);
	}

	inline ExecutableSceneSwitch(OBSWeakSource scene,
				     OBSWeakSource transition,
				     const QString &exe, bool inFocus)
		: mScene(scene),
		  mTransition(transition),
		  mExe(exe),
		  mInFocus(inFocus)
	{
	}
};
