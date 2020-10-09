#pragma once
#include <string>
#include <obs.hpp>

struct SceneSwitcherEntry {
	OBSWeakSource scene = nullptr;
	OBSWeakSource transition = nullptr;
	bool usePreviousScene = false;

	virtual bool valid()
	{
		return (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(transition);
	}

	virtual const char *getType() = 0;

	// TODO
	//virtual bool checkMatch() = 0;

	virtual void logMatch()
	{
		obs_source_t *s = obs_weak_source_get_source(scene);
		const char *sceneName = obs_source_get_name(s);
		obs_source_release(s);
		blog(LOG_INFO,
		     "[adv-ss] match for '%s' - switch to scene '%s'",
		     getType(), sceneName);
	}

	inline SceneSwitcherEntry() {}

	inline SceneSwitcherEntry(OBSWeakSource scene_,
				  OBSWeakSource transition_,
				  bool usePreviousScene_)
		: scene(scene_),
		  transition(transition_),
		  usePreviousScene(usePreviousScene_)
	{
	}
	inline SceneSwitcherEntry(OBSWeakSource scene_,
				  OBSWeakSource transition_)
		: scene(scene_),
		  transition(transition_),
		  usePreviousScene(false)
	{
	}
};
