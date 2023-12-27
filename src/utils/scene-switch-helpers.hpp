#pragma once
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

void SwitchScene(const SceneSwitchInfo &, bool force = false);
void SwitchPreviewScene(const OBSWeakSource &scene);

std::chrono::high_resolution_clock::time_point GetLastTransitionEndTime();
std::chrono::high_resolution_clock::time_point GetLastSceneChangeTime();

OBSWeakSource GetCurrentScene();
OBSWeakSource GetPreviousScene();

bool ShouldModifyTransitionOverrides();
bool ShouldModifyActiveTransition();
void OverwriteTransitionOverride(const SceneSwitchInfo &, TransitionData &);
void RestoreTransitionOverride(obs_source_t *scene, const TransitionData &);

bool AnySceneTransitionStarted();

// TODO: Remove at some point
void SetNextTransition(const SceneSwitchInfo &, obs_source_t *currentSource,
		       TransitionData &);

} // namespace advss
