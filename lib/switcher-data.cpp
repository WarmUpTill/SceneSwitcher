#include "switcher-data.hpp"
#include "source-helpers.hpp"

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

std::unique_lock<std::mutex> *GetSwitcherLoopLock()
{
	return switcher ? switcher->mainLoopLock : nullptr;
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
		switchIfNotMatching = NoMatchBehavior::NO_SWITCH;
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

void SwitcherData::RunPostLoadSteps()
{
	for (const auto &func : postLoadSteps) {
		func();
	}
	postLoadSteps.clear();
}

void SwitcherData::AddSaveStep(std::function<void(obs_data_t *)> function)
{
	std::lock_guard<std::mutex> lock(switcher->m);
	saveSteps.emplace_back(function);
}

void SwitcherData::AddLoadStep(std::function<void(obs_data_t *)> function)
{
	std::lock_guard<std::mutex> lock(switcher->m);
	loadSteps.emplace_back(function);
}

void SwitcherData::AddPostLoadStep(std::function<void()> function)
{
	postLoadSteps.emplace_back(function);
}

static void startHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (!(switcher->th && switcher->th->isRunning())) {
			switcher->Start();
		}
	}
}

static void stopHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		}
	}
}

static void startStopToggleHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				      bool pressed)
{
	if (pressed) {
		if (switcher->th && switcher->th->isRunning()) {
			switcher->Stop();
		} else {
			switcher->Start();
		}
	}
}

QWidget *GetSettingsWindow();

static void upMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				     bool pressed)
{
	if (pressed && SettingsWindowIsOpened()) {
		QMetaObject::invokeMethod(GetSettingsWindow(),
					  "UpMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

static void downMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
				       bool pressed)
{
	if (pressed && SettingsWindowIsOpened()) {
		QMetaObject::invokeMethod(GetSettingsWindow(),
					  "DownMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

static void removeMacroSegmentHotkeyFunc(void *, obs_hotkey_id, obs_hotkey_t *,
					 bool pressed)
{
	if (pressed && SettingsWindowIsOpened()) {
		QMetaObject::invokeMethod(GetSettingsWindow(),
					  "DeleteMacroSegementHotkey",
					  Qt::QueuedConnection);
	}
}

static void registerHotkeys()
{
	switcher->startHotkey = obs_hotkey_register_frontend(
		"startSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.startSwitcherHotkey"),
		startHotkeyFunc, NULL);
	switcher->stopHotkey = obs_hotkey_register_frontend(
		"stopSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.stopSwitcherHotkey"),
		stopHotkeyFunc, NULL);
	switcher->toggleHotkey = obs_hotkey_register_frontend(
		"startStopToggleSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.startStopToggleSwitcherHotkey"),
		startStopToggleHotkeyFunc, NULL);
	switcher->upMacroSegment = obs_hotkey_register_frontend(
		"upMacroSegmentSwitcherHotkey",
		obs_module_text("AdvSceneSwitcher.hotkey.upMacroSegmentHotkey"),
		upMacroSegmentHotkeyFunc, NULL);
	switcher->downMacroSegment = obs_hotkey_register_frontend(
		"downMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.downMacroSegmentHotkey"),
		downMacroSegmentHotkeyFunc, NULL);
	switcher->removeMacroSegment = obs_hotkey_register_frontend(
		"removeMacroSegmentSwitcherHotkey",
		obs_module_text(
			"AdvSceneSwitcher.hotkey.removeMacroSegmentHotkey"),
		removeMacroSegmentHotkeyFunc, NULL);

	switcher->hotkeysRegistered = true;
}

static void saveHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_hotkey_save(id);
	obs_data_set_array(obj, name, a);
	obs_data_array_release(a);
}

void SwitcherData::SaveHotkeys(obs_data_t *obj)
{
	saveHotkey(obj, startHotkey, "startHotkey");
	saveHotkey(obj, stopHotkey, "stopHotkey");
	saveHotkey(obj, toggleHotkey, "toggleHotkey");
	saveHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	saveHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	saveHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}

static void loadHotkey(obs_data_t *obj, obs_hotkey_id id, const char *name)
{
	obs_data_array_t *a = obs_data_get_array(obj, name);
	obs_hotkey_load(id, a);
	obs_data_array_release(a);
}

void SwitcherData::LoadHotkeys(obs_data_t *obj)
{
	if (!hotkeysRegistered) {
		registerHotkeys();
	}
	loadHotkey(obj, startHotkey, "startHotkey");
	loadHotkey(obj, stopHotkey, "stopHotkey");
	loadHotkey(obj, toggleHotkey, "toggleHotkey");
	loadHotkey(obj, upMacroSegment, "upMacroSegmentHotkey");
	loadHotkey(obj, downMacroSegment, "downMacroSegmentHotkey");
	loadHotkey(obj, removeMacroSegment, "removeMacroSegmentHotkey");
}

} // namespace advss
