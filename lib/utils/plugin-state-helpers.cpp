#include "plugin-state-helpers.hpp"
#include "advanced-scene-switcher.hpp"
#include "switcher-data.hpp"

namespace advss {

static std::vector<std::function<void()>> &getPluginInitSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::vector<std::function<void()>> &getPluginPostLoadSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::vector<std::function<void()>> &getPluginCleanupSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::vector<std::function<void()>> &getResetIntervalSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::vector<std::function<void()>> &getStartSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::vector<std::function<void()>> &getStopSteps()
{
	static std::vector<std::function<void()>> steps;
	return steps;
}

static std::mutex mutex;

void SavePluginSettings(obs_data_t *obj)
{
	GetSwitcher()->SaveSettings(obj);
}

void LoadPluginSettings(obs_data_t *obj)
{
	GetSwitcher()->LoadSettings(obj);
}

void AddSaveStep(std::function<void(obs_data_t *)> step)
{
	GetSwitcher()->AddSaveStep(step);
}

void AddLoadStep(std::function<void(obs_data_t *)> step)
{
	GetSwitcher()->AddLoadStep(step);
}

void AddPostLoadStep(std::function<void()> step)
{
	GetSwitcher()->AddPostLoadStep(step);
}

void AddIntervalResetStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getResetIntervalSteps().emplace_back(step);
}

void RunPostLoadSteps()
{
	GetSwitcher()->RunPostLoadSteps();
}

void AddPluginInitStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getPluginInitSteps().emplace_back(step);
}

void AddPluginPostLoadStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getPluginPostLoadSteps().emplace_back(step);
}

void AddPluginCleanupStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getPluginCleanupSteps().emplace_back(step);
}

void RunPluginInitSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getPluginInitSteps()) {
		step();
	}
}

void RunPluginPostLoadSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getPluginPostLoadSteps()) {
		step();
	}
}

void RunPluginCleanupSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getPluginCleanupSteps()) {
		step();
	}
}

void RunIntervalResetSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getResetIntervalSteps()) {
		step();
	}
}

void AddStartStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getStartSteps().emplace_back(step);
}

void AddStopStep(std::function<void()> step)
{
	std::lock_guard<std::mutex> lock(mutex);
	getStopSteps().emplace_back(step);
}

void RunStartSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getStartSteps()) {
		step();
	}
}

void RunStopSteps()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &step : getStopSteps()) {
		step();
	}
}

void StopPlugin()
{
	GetSwitcher()->Stop();
}

void StartPlugin()
{
	GetSwitcher()->Start();
}

bool PluginIsRunning()
{
	return GetSwitcher() && GetSwitcher()->th &&
	       GetSwitcher()->th->isRunning();
}

int GetIntervalValue()
{
	return GetSwitcher()->interval;
}

void SetPluginNoMatchBehavior(NoMatchBehavior behavior)
{
	GetSwitcher()->switchIfNotMatching = behavior;
}

NoMatchBehavior GetPluginNoMatchBehavior()
{
	return GetSwitcher()->switchIfNotMatching;
}

void SetNoMatchScene(const OBSWeakSource &scene)
{
	GetSwitcher()->nonMatchingScene = scene;
}

std::string ForegroundWindowTitle()
{
	return GetSwitcher()->currentTitle;
}

std::string PreviousForegroundWindowTitle()
{
	return GetSwitcher()->lastTitle;
}

bool SettingsWindowIsOpened()
{
	return GetSwitcher()->settingsWindowOpened;
}

bool HighlightUIElementsEnabled()
{
	return GetSwitcher() && !GetSwitcher()->disableHints;
}

bool OBSIsShuttingDown()
{
	return GetSwitcher() && GetSwitcher()->obsIsShuttingDown;
}

bool InitialLoadIsComplete()
{
	return GetSwitcher()->startupLoadDone;
}

bool IsFirstInterval()
{
	return GetSwitcher()->firstInterval;
}

bool IsFirstIntervalAfterStop()
{
	return GetSwitcher()->firstIntervalAfterStop;
}

void SetMacroHighlightingEnabled(bool enable)
{
	auto &settings = GetGlobalMacroSettings();
	settings._highlightActions = enable;
	settings._highlightConditions = enable;
	settings._highlightExecuted = enable;

	obs_queue_task(
		OBS_TASK_UI,
		[](void *) {
			if (!SettingsWindowIsOpened()) {
				return;
			}
			AdvSceneSwitcher::window->HighlightMacrosChanged(
				GetGlobalMacroSettings()._highlightExecuted);
		},
		nullptr, false);
}

bool IsMacroHighlightingEnabled()
{
	const auto &settings = GetGlobalMacroSettings();
	return settings._highlightActions || settings._highlightConditions ||
	       settings._highlightExecuted;
}

} // namespace advss
