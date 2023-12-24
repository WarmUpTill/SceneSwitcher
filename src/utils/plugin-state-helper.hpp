#pragma once
#include <functional>
#include <obs.hpp>
#include <string>

namespace advss {

void LoadPluginSettings(obs_data_t *);

void AddSaveStep(std::function<void(obs_data_t *)>);
void AddLoadStep(std::function<void(obs_data_t *)>);
void AddPostLoadStep(std::function<void()>);
void AddIntervalResetStep(std::function<void()>, bool lock = true);

void StopPlugin();
void StartPlugin();
bool PluginIsRunning();
int GetIntervalValue();

enum class NoMatchBehavior { NO_SWITCH = 0, SWITCH = 1, RANDOM_SWITCH = 2 };
void SetPluginNoMatchBehavior(NoMatchBehavior);
NoMatchBehavior GetPluginNoMatchBehavior();
void SetNoMatchScene(const OBSWeakSource &);

std::string ForegroundWindowTitle();
std::string PreviousForegroundWindowTitle();

bool SettingsWindowIsOpened();
bool HighlightUIElementsEnabled();

bool OBSIsShuttingDown();
bool InitialLoadIsComplete();
bool IsFirstInterval();
bool IsFirstIntervalAfterStop();

} // namespace advss
