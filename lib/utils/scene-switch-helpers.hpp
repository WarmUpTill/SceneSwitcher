#pragma once
#include "export-symbol-helper.hpp"

#include <obs.hpp>
#include <chrono>
#include <string>

namespace advss {

struct SceneSwitchInfo {
	OBSWeakSource scene;
	OBSWeakSource transition;
	int duration = 0;
};

struct TransitionData {
	std::string name = "";
	int duration = 0;
};

EXPORT void SwitchScene(const SceneSwitchInfo &, bool force = false);
void SwitchPreviewScene(const OBSWeakSource &scene);

EXPORT std::chrono::high_resolution_clock::time_point
GetLastTransitionEndTime();
EXPORT std::chrono::high_resolution_clock::time_point GetLastSceneChangeTime();

EXPORT OBSWeakSource GetCurrentScene();
EXPORT OBSWeakSource GetPreviousScene();

EXPORT bool ShouldModifyTransitionOverrides();
bool ShouldModifyActiveTransition();
void OverwriteTransitionOverride(const SceneSwitchInfo &, TransitionData &);
void RestoreTransitionOverride(obs_source_t *scene, const TransitionData &);

EXPORT bool AnySceneTransitionStarted();

// TODO: Remove at some point
void SetNextTransition(const SceneSwitchInfo &, obs_source_t *currentSource,
		       TransitionData &);

} // namespace advss
