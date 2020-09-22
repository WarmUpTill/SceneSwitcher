#include "headers/switcher-data-structs.hpp"

void SwitcherData::Prune()
{
	for (size_t i = 0; i < windowSwitches.size(); i++) {
		WindowSceneSwitch &s = windowSwitches[i];
		if (!s.valid())
			windowSwitches.erase(windowSwitches.begin() + i--);
	}

	if (nonMatchingScene && !WeakSourceValid(nonMatchingScene)) {
		switchIfNotMatching = NO_SWITCH;
		nonMatchingScene = nullptr;
	}

	for (size_t i = 0; i < randomSwitches.size(); i++) {
		RandomSwitch &s = randomSwitches[i];
		if (!s.valid())
			randomSwitches.erase(randomSwitches.begin() + i--);
	}

	for (size_t i = 0; i < screenRegionSwitches.size(); i++) {
		ScreenRegionSwitch &s = screenRegionSwitches[i];
		if (!s.valid())
			screenRegionSwitches.erase(
				screenRegionSwitches.begin() + i--);
	}

	for (size_t i = 0; i < pauseScenesSwitches.size(); i++) {
		OBSWeakSource &scene = pauseScenesSwitches[i];
		if (!WeakSourceValid(scene))
			pauseScenesSwitches.erase(pauseScenesSwitches.begin() +
						  i--);
	}

	for (size_t i = 0; i < sceneRoundTripSwitches.size(); i++) {
		SceneRoundTripSwitch &s = sceneRoundTripSwitches[i];
		if (!s.valid())
			sceneRoundTripSwitches.erase(
				sceneRoundTripSwitches.begin() + i--);
	}

	if (!WeakSourceValid(autoStopScene)) {
		autoStopScene = nullptr;
		autoStopEnable = false;
	}

	for (size_t i = 0; i < sceneTransitions.size(); i++) {
		SceneTransition &s = sceneTransitions[i];
		if (!s.valid())
			sceneTransitions.erase(sceneTransitions.begin() + i--);
	}

	for (size_t i = 0; i < defaultSceneTransitions.size(); i++) {
		DefaultSceneTransition &s = defaultSceneTransitions[i];
		if (!s.valid())
			defaultSceneTransitions.erase(
				defaultSceneTransitions.begin() + i--);
	}

	for (size_t i = 0; i < executableSwitches.size(); i++) {
		ExecutableSceneSwitch &s = executableSwitches[i];
		if (!s.valid())
			executableSwitches.erase(executableSwitches.begin() +
						 i--);
	}

	for (size_t i = 0; i < fileSwitches.size(); i++) {
		FileSwitch &s = fileSwitches[i];
		if (!s.valid())
			fileSwitches.erase(fileSwitches.begin() + i--);
	}

	for (size_t i = 0; i < timeSwitches.size(); i++) {
		TimeSwitch &s = timeSwitches[i];
		if (!s.valid())
			timeSwitches.erase(timeSwitches.begin() + i--);
	}

	if (!idleData.valid()) {
		idleData.idleEnable = false;
	}

	for (size_t i = 0; i < mediaSwitches.size(); i++) {
		MediaSwitch &s = mediaSwitches[i];
		if (!s.valid())
			mediaSwitches.erase(mediaSwitches.begin() + i--);
	}

	for (size_t i = 0; i < audioSwitches.size(); i++) {
		AudioSwitch &s = audioSwitches[i];
		if (!s.valid())
			audioSwitches.erase(audioSwitches.begin() + i--);
	}
}
