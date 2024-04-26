#pragma once
#include <functional>
#include <obs.hpp>
#include <string>

namespace advss {

void SavePluginSettings(obs_data_t *);
EXPORT void LoadPluginSettings(obs_data_t *);

EXPORT void AddSaveStep(std::function<void(obs_data_t *)>);
EXPORT void AddLoadStep(std::function<void(obs_data_t *)>);
EXPORT void AddPostLoadStep(std::function<void()>);
EXPORT void AddIntervalResetStep(std::function<void()>, bool lock = true);
EXPORT void RunPostLoadSteps();

EXPORT void AddPluginInitStep(std::function<void()>);
EXPORT void AddPluginPostLoadStep(std::function<void()>);
EXPORT void AddPluginCleanupStep(std::function<void()>);
void RunPluginInitSteps();
extern "C" EXPORT void RunPluginPostLoadSteps();
void RunPluginCleanupSteps();

EXPORT void StopPlugin();
EXPORT void StartPlugin();
EXPORT bool PluginIsRunning();
EXPORT int GetIntervalValue();

enum class NoMatchBehavior { NO_SWITCH = 0, SWITCH = 1, RANDOM_SWITCH = 2 };
EXPORT void SetPluginNoMatchBehavior(NoMatchBehavior);
EXPORT NoMatchBehavior GetPluginNoMatchBehavior();
EXPORT void SetNoMatchScene(const OBSWeakSource &);

EXPORT std::string ForegroundWindowTitle();
EXPORT std::string PreviousForegroundWindowTitle();

EXPORT bool SettingsWindowIsOpened();
EXPORT bool HighlightUIElementsEnabled();

EXPORT bool OBSIsShuttingDown();
EXPORT bool InitialLoadIsComplete();
EXPORT bool IsFirstInterval();
EXPORT bool IsFirstIntervalAfterStop();

} // namespace advss
