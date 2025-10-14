#include "scene-switch-helpers.hpp"
#include "canvas-helpers.hpp"
#include "source-helpers.hpp"
#include "switcher-data.hpp"

#include <obs-frontend-api.h>
#include <obs-websocket-api.h>

#undef blog
#include "log-helper.hpp"

namespace advss {

static std::chrono::high_resolution_clock::time_point lastSceneChangeTime{};
static std::chrono::high_resolution_clock::time_point lastTransitionEndTime{};
static bool setupEventHandler();
static bool eventHandlerIsSetup = setupEventHandler();

static bool setupEventHandler()
{
	static auto handleEvents = [](enum obs_frontend_event event, void *) {
		switch (event) {
		case OBS_FRONTEND_EVENT_SCENE_CHANGED:
			lastSceneChangeTime =
				std::chrono::high_resolution_clock::now();
			break;
		case OBS_FRONTEND_EVENT_TRANSITION_STOPPED:
			lastTransitionEndTime =
				std::chrono::high_resolution_clock::now();
			break;
		default:
			break;
		};
	};
	obs_frontend_add_event_callback(handleEvents, nullptr);
	return true;
}

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
	OBSWeakSourceAutoRelease currentScene =
		obs_source_get_weak_source(currentSource);
	auto tinfo = getNextTransition(currentScene, sceneSwitch.scene);

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
		OBSSourceAutoRelease currentTransition =
			obs_frontend_get_current_transition();
		nextTransition = obs_source_get_weak_source(currentTransition);
		obs_weak_source_release(nextTransition);
	}
	if (!nextTransitionDuration) {
		nextTransitionDuration = obs_frontend_get_transition_duration();
	}

	if (ShouldModifyActiveTransition()) {
		obs_frontend_set_transition_duration(nextTransitionDuration);
		OBSSourceAutoRelease transition =
			obs_weak_source_get_source(nextTransition);
		obs_frontend_set_current_transition(transition);
	}

	if (ShouldModifyTransitionOverrides()) {
		OverwriteTransitionOverride({sceneSwitch.scene, nextTransition,
					     nextTransitionDuration},
					    td);
	}
}

void OverwriteTransitionOverride(const SceneSwitchInfo &ssi, TransitionData &td)
{
	OBSSourceAutoRelease scene = obs_weak_source_get_source(ssi.scene);
	OBSDataAutoRelease data = obs_source_get_private_settings(scene);

	td.name = obs_data_get_string(data, "transition");
	td.duration = obs_data_get_int(data, "transition_duration");

	std::string name = GetWeakSourceName(ssi.transition);

	obs_data_set_string(data, "transition", name.c_str());
	obs_data_set_int(data, "transition_duration", ssi.duration);
}

void RestoreTransitionOverride(obs_source_t *scene, const TransitionData &td)
{
	OBSDataAutoRelease data = obs_source_get_private_settings(scene);
	obs_data_set_string(data, "transition", td.name.c_str());
	obs_data_set_int(data, "transition_duration", td.duration);
}

static bool modifiesTransitionSettings(const SceneSwitchInfo &ssi)
{
	return ssi.duration != 0 || ssi.transition;
}

void SwitchScene(const SceneSwitchInfo &sceneSwitch, bool force,
		 obs_weak_canvas_t *canvas)
{
	if (!sceneSwitch.scene) {
		vblog(LOG_INFO, "nothing to switch to");
		return;
	}

	OBSSourceAutoRelease source =
		obs_weak_source_get_source(sceneSwitch.scene);
	OBSSourceAutoRelease currentSource = obs_frontend_get_current_scene();

	if (!source || (source == currentSource && !force)) {
		return;
	}

	TransitionData currentTransitionData;
	const bool modifiesTransition = modifiesTransitionSettings(sceneSwitch);

	if (!canvas || IsMainCanvas(canvas)) { // Assume main canvas by default
		if (modifiesTransition) {
			SetNextTransition(sceneSwitch, currentSource,
					  currentTransitionData);
		}
		obs_frontend_set_current_scene(source);
		if (modifiesTransition && ShouldModifyTransitionOverrides()) {
			RestoreTransitionOverride(source,
						  currentTransitionData);
		}
		vblog(LOG_INFO, "switched main canvas scene");
		return;
	}

	if (GetWeakCanvasName(canvas) == "Aitum Vertical") {
		// Can't set transitions so we ignore these options
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, "vendorName",
				    "aitum-vertical-canvas");
		obs_data_set_string(data, "requestType", "switch_scene");
		OBSDataAutoRelease requestData = obs_data_create();
		obs_data_set_string(requestData, "scene",
				    obs_source_get_name(source));
		obs_data_set_obj(data, "requestData", requestData);
		obs_websocket_request_response_free(
			obs_websocket_call_request("CallVendorRequest", data));
		vblog(LOG_INFO, "switched Aitum Vertical canvas scene");
	}
}

void SwitchPreviewScene(const OBSWeakSource &ws)
{
	OBSSourceAutoRelease source = obs_weak_source_get_source(ws);
	obs_frontend_set_current_preview_scene(source);
}

std::chrono::high_resolution_clock::time_point GetLastTransitionEndTime()
{
	return lastTransitionEndTime;
}

std::chrono::high_resolution_clock::time_point GetLastSceneChangeTime()
{
	return lastSceneChangeTime;
}

OBSWeakSource GetCurrentScene()
{
	return switcher->currentScene;
}

OBSWeakSource GetPreviousScene()
{
	return switcher->previousScene;
}

bool AnySceneTransitionStarted()
{
	return switcher->AnySceneTransitionStarted();
}

bool ShouldModifyTransitionOverrides()
{
	return switcher->transitionOverrideOverride;
}

bool ShouldModifyActiveTransition()
{
	return switcher->adjustActiveTransitionType;
}

} // namespace advss
