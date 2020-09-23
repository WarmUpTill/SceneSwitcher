#pragma once
#include <string>
#include "utility.hpp"
#include "switch-generic.hpp"

struct SceneTransition : SceneSwitcherEntry {
	OBSWeakSource scene2;
	std::string sceneTransitionStr;

	const char *getType() { return "transition"; }

	bool valid()
	{
		return (WeakSourceValid(scene) && WeakSourceValid(scene2)) &&
		       WeakSourceValid(transition);
	}

	inline SceneTransition(OBSWeakSource scene_, OBSWeakSource scene2_,
			       OBSWeakSource transition_,
			       std::string sceneTransitionStr_)
		: SceneSwitcherEntry(scene_, transition_),
		  scene2(scene2_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct DefaultSceneTransition : SceneSwitcherEntry {
	std::string sceneTransitionStr;

	const char *getType() { return "def_transition"; }

	inline DefaultSceneTransition(OBSWeakSource scene_,
				      OBSWeakSource transition_,
				      std::string sceneTransitionStr_)
		: SceneSwitcherEntry(scene_, transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};
