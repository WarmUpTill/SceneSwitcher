#pragma once
#include <string>
#include <obs.hpp>

struct SceneTransition {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	OBSWeakSource transition;
	std::string sceneTransitionStr;

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

	inline DefaultSceneTransition(OBSWeakSource scene_,
				      OBSWeakSource transition_,
				      std::string sceneTransitionStr_)
		: scene(scene_),
		  transition(transition_),
		  sceneTransitionStr(sceneTransitionStr_)
	{
	}
};
