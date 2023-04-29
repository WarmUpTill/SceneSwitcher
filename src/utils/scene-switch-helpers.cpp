#include "scene-switch-helpers.hpp"
#include "switcher-data.hpp"
#include "obs-frontend-api.h"
#include "utility.hpp"

namespace advss {

static std::pair<obs_weak_source_t *, int>
getNextTransition(obs_weak_source_t *scene1, obs_weak_source_t *scene2)
{
	obs_weak_source_t *ws = nullptr;
	int duration = 0;
	if (scene1 && scene2) {
		for (SceneTransition &t : switcher->sceneTransitions) {
			if (!t.initialized()) {
				continue;
			}

			if (t.scene == scene1 && t.scene2 == scene2) {
				ws = t.transition;
				duration = t.duration * 1000;
				break;
			}
		}
	}
	return std::make_pair(ws, duration);
}

void SetNextTransition(const SceneSwitchInfo &sceneSwitch,
		       obs_source_t *currentSource, TransitionData &td)
{
	// Priority:
	// 1. Transition tab
	// 2. Individual switcher entry
	// 3. Current transition settings

	// Transition Tab
	obs_weak_source_t *currentScene =
		obs_source_get_weak_source(currentSource);
	auto tinfo = getNextTransition(currentScene, sceneSwitch.scene);
	obs_weak_source_release(currentScene);

	OBSWeakSource nextTransition = tinfo.first;
	int nextTransitionDuration = tinfo.second;

	// Individual switcher entry
	if (!nextTransition) {
		nextTransition = sceneSwitch.transition;
	}
	if (!nextTransitionDuration) {
		nextTransitionDuration = sceneSwitch.duration;
	}

	// Current transition settings
	if (!nextTransition) {
		auto ct = obs_frontend_get_current_transition();
		nextTransition = obs_source_get_weak_source(ct);
		obs_weak_source_release(nextTransition);
		obs_source_release(ct);
	}
	if (!nextTransitionDuration) {
		nextTransitionDuration = obs_frontend_get_transition_duration();
	}

	if (switcher->adjustActiveTransitionType) {
		obs_frontend_set_transition_duration(nextTransitionDuration);
		auto t = obs_weak_source_get_source(nextTransition);
		obs_frontend_set_current_transition(t);
		obs_source_release(t);
	}

	if (switcher->transitionOverrideOverride) {
		OverwriteTransitionOverride({sceneSwitch.scene, nextTransition,
					     nextTransitionDuration},
					    td);
	}
}

void OverwriteTransitionOverride(const SceneSwitchInfo &ssi, TransitionData &td)
{
	obs_source_t *scene = obs_weak_source_get_source(ssi.scene);
	obs_data_t *data = obs_source_get_private_settings(scene);

	td.name = obs_data_get_string(data, "transition");
	td.duration = obs_data_get_int(data, "transition_duration");

	std::string name = GetWeakSourceName(ssi.transition);

	obs_data_set_string(data, "transition", name.c_str());
	obs_data_set_int(data, "transition_duration", ssi.duration);

	obs_data_release(data);
	obs_source_release(scene);
}

void RestoreTransitionOverride(obs_source_t *scene, const TransitionData &td)
{
	obs_data_t *data = obs_source_get_private_settings(scene);

	obs_data_set_string(data, "transition", td.name.c_str());
	obs_data_set_int(data, "transition_duration", td.duration);

	obs_data_release(data);
}

void SwitchScene(const SceneSwitchInfo &sceneSwitch, bool force)
{
	if (!sceneSwitch.scene) {
		vblog(LOG_INFO, "nothing to switch to");
		return;
	}

	obs_source_t *source = obs_weak_source_get_source(sceneSwitch.scene);
	obs_source_t *currentSource = obs_frontend_get_current_scene();

	if (source && (source != currentSource || force)) {
		TransitionData currentTransitionData;
		SetNextTransition(sceneSwitch, currentSource,
				  currentTransitionData);
		obs_frontend_set_current_scene(source);
		if (switcher->transitionOverrideOverride) {
			RestoreTransitionOverride(source,
						  currentTransitionData);
		}

		if (switcher->verbose) {
			blog(LOG_INFO, "switched scene");
		}

		if (switcher->networkConfig.ShouldSendSceneChange()) {
			switcher->server.sendMessage(sceneSwitch);
		}
	}
	obs_source_release(currentSource);
	obs_source_release(source);
}

void SwitchPreviewScene(const OBSWeakSource &ws)
{
	auto source = obs_weak_source_get_source(ws);
	obs_frontend_set_current_preview_scene(source);
	obs_source_release(source);
}

} // namespace advss
