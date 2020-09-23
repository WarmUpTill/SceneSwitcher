#pragma once
#include <string>
#include <obs.hpp>

struct SceneSwitcherEntry {
	OBSWeakSource scene;
	OBSWeakSource transition;
	bool usePreviousScene;

	virtual bool SceneSwitcherEntry::valid()
	{
		return (usePreviousScene || WeakSourceValid(scene)) &&
		       WeakSourceValid(transition);
	}

	virtual const char* getType() { return "none"; }

	virtual void SceneSwitcherEntry::logMatch()
	{
		obs_source_t *s = obs_weak_source_get_source(scene);
		const char* name = obs_source_get_name(s);
		obs_source_release(s);
		blog(LOG_INFO,
		     "Advanced Scene Switcher match for %s for scene %s",
		     getType(), name);
	}

	inline SceneSwitcherEntry() : usePreviousScene(false) {}

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
