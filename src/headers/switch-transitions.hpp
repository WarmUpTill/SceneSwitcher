#pragma once
#include <string>
#include "utility.hpp"

struct SceneTransition {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	std::string sceneTransitionStr;

	bool valid()
	{
		return (WeakSourceValid(scene1) && WeakSourceValid(scene2)) &&
		       WeakSourceValid(transition);
	}

	inline SceneTransition(OBSWeakSource scene1_, OBSWeakSource scene2_,
			       OBSWeakSource transition_,
			       std::string sceneTransitionStr_)
		: scene1(scene1_),
		  scene2(scene2_),
		  transition(transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};

struct DefaultSceneTransition {
	OBSWeakSource scene;
	OBSWeakSource transition;
	std::string sceneTransitionStr;

	bool valid()
	{
		return WeakSourceValid(scene) && WeakSourceValid(transition);
	}

	inline DefaultSceneTransition(OBSWeakSource scene_,
				      OBSWeakSource transition_,
				      std::string sceneTransitionStr_)
		: scene(scene_),
		  transition(transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};
