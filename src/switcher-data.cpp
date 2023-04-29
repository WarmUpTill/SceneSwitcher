#include "switcher-data.hpp"
#include "utility.hpp"

namespace advss {

SwitcherData *switcher = nullptr;
SwitcherData *GetSwitcher()
{
	return switcher;
}

std::mutex *GetSwitcherMutex()
{
	return switcher ? &switcher->m : nullptr;
}

bool VerboseLoggingEnabled()
{
	return switcher ? switcher->verbose : false;
}

SwitcherData::SwitcherData(obs_module_t *m, translateFunc t)
{
	_modulePtr = m;
	_translate = t;
}

const char *SwitcherData::Translate(const char *text)
{
	return _translate(text);
}

obs_module_t *SwitcherData::GetModule()
{
	return _modulePtr;
}

void SwitcherData::Prune()
{
	for (size_t i = 0; i < windowSwitches.size(); i++) {
		WindowSwitch &s = windowSwitches[i];
		if (!s.valid()) {
			windowSwitches.erase(windowSwitches.begin() + i--);
		}
	}

	if (nonMatchingScene && !WeakSourceValid(nonMatchingScene)) {
		switchIfNotMatching = NoMatch::NO_SWITCH;
		nonMatchingScene = nullptr;
	}

	for (size_t i = 0; i < randomSwitches.size(); i++) {
		RandomSwitch &s = randomSwitches[i];
		if (!s.valid()) {
			randomSwitches.erase(randomSwitches.begin() + i--);
		}
	}

	for (size_t i = 0; i < screenRegionSwitches.size(); i++) {
		ScreenRegionSwitch &s = screenRegionSwitches[i];
		if (!s.valid()) {
			screenRegionSwitches.erase(
				screenRegionSwitches.begin() + i--);
		}
	}

	for (size_t i = 0; i < pauseEntries.size(); i++) {
		PauseEntry &s = pauseEntries[i];
		if (!s.valid()) {
			pauseEntries.erase(pauseEntries.begin() + i--);
		}
	}

	for (size_t i = 0; i < sceneSequenceSwitches.size(); i++) {
		SceneSequenceSwitch &s = sceneSequenceSwitches[i];
		if (!s.valid()) {
			sceneSequenceSwitches.erase(
				sceneSequenceSwitches.begin() + i--);
		}

		auto cur = &s;
		while (cur != nullptr) {
			if (cur->extendedSequence &&
			    !cur->extendedSequence->valid()) {
				cur->extendedSequence.reset(nullptr);
				s.activeSequence = nullptr;
			}
			cur = cur->extendedSequence.get();
		}
	}

	for (size_t i = 0; i < sceneTransitions.size(); i++) {
		SceneTransition &s = sceneTransitions[i];
		if (!s.valid()) {
			sceneTransitions.erase(sceneTransitions.begin() + i--);
		}
	}

	for (size_t i = 0; i < defaultSceneTransitions.size(); i++) {
		DefaultSceneTransition &s = defaultSceneTransitions[i];
		if (!s.valid()) {
			defaultSceneTransitions.erase(
				defaultSceneTransitions.begin() + i--);
		}
	}

	for (size_t i = 0; i < executableSwitches.size(); i++) {
		ExecutableSwitch &s = executableSwitches[i];
		if (!s.valid()) {
			executableSwitches.erase(executableSwitches.begin() +
						 i--);
		}
	}

	for (size_t i = 0; i < fileSwitches.size(); i++) {
		FileSwitch &s = fileSwitches[i];
		if (!s.valid()) {
			fileSwitches.erase(fileSwitches.begin() + i--);
		}
	}

	for (size_t i = 0; i < timeSwitches.size(); i++) {
		TimeSwitch &s = timeSwitches[i];
		if (!s.valid()) {
			timeSwitches.erase(timeSwitches.begin() + i--);
		}
	}

	if (!idleData.valid()) {
		idleData.idleEnable = false;
	}

	for (size_t i = 0; i < mediaSwitches.size(); i++) {
		MediaSwitch &s = mediaSwitches[i];
		if (!s.valid()) {
			mediaSwitches.erase(mediaSwitches.begin() + i--);
		}
	}

	for (size_t i = 0; i < audioSwitches.size(); i++) {
		AudioSwitch &s = audioSwitches[i];
		if (!s.valid()) {
			audioSwitches.erase(audioSwitches.begin() + i--);
		}
	}

	for (auto &sg : sceneGroups) {
		for (size_t i = 0; i < sg.scenes.size(); i++)
			if (!WeakSourceValid(sg.scenes[i])) {
				sg.scenes.erase(sg.scenes.begin() + i--);
			}
	}
}

bool SwitcherData::VersionChanged(obs_data_t *obj, std::string currentVersion)
{
	if (!obs_data_has_user_value(obj, "version")) {
		return false;
	}
	switcher->firstBoot = false;
	std::string previousVersion = obs_data_get_string(obj, "version");
	return previousVersion != currentVersion;
}

void SwitcherData::SaveVersion(obs_data_t *obj,
			       const std::string &currentVersion)
{
	obs_data_set_string(obj, "version", currentVersion.c_str());
}

void SwitcherData::AddResetForNextIntervalFunction(
	std::function<void()> function)
{
	std::lock_guard<std::mutex> lock(switcher->m);
	resetForNextIntervalFuncs.emplace_back(function);
}

} // namespace advss
