#pragma once
#include <obs.hpp>
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

void SetNextTransition(const SceneSwitchInfo &ssi, obs_source_t *currentSource,
		       TransitionData &td);
void OverwriteTransitionOverride(const SceneSwitchInfo &ssi,
				 TransitionData &td);
void RestoreTransitionOverride(obs_source_t *scene, const TransitionData &td);
void SwitchScene(const SceneSwitchInfo &ssi, bool force = false);
void SwitchPreviewScene(const OBSWeakSource &ws);

} // namespace advss
