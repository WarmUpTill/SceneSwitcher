#include "transition-helpers.hpp"

namespace advss {

bool IsFixedLengthTransition(const OBSWeakSource &transition)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(transition);
	return obs_transition_fixed(source);
}

obs_source_t *SetSceneItemTransition(const OBSSceneItem &item,
				     const OBSSourceAutoRelease &transition,
				     bool show)
{
	OBSDataAutoRelease settings = obs_source_get_settings(transition);
	if (!transition || !settings) {
		// Set transition to "None"
		obs_sceneitem_set_transition(item, show, nullptr);
		return nullptr;
	}

	// We cannot share the transition source between
	// scene items without introducing strange graphical
	// artifacts so we have to create new ones here
	OBSSourceAutoRelease transitionSource = obs_source_create_private(
		obs_source_get_id(transition), obs_source_get_name(transition),
		settings);
	obs_sceneitem_set_transition(item, show, transitionSource);
	return transitionSource.Get();
}

} // namespace advss
