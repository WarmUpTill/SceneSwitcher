#pragma once
#include <functional>
#include <obs.hpp>

namespace advss {

void LoadPluginSettings(obs_data_t *);

void AddSaveStep(std::function<void(obs_data_t *)>);
void AddLoadStep(std::function<void(obs_data_t *)>);
void AddPostLoadStep(std::function<void()>);
void AddIntervalResetStep(std::function<void()>);

void StopPlugin();
void StartPlugin();

enum class NoMatchBehavior { NO_SWITCH = 0, SWITCH = 1, RANDOM_SWITCH = 2 };
void SetPluginNoMatchBehavior(NoMatchBehavior);
NoMatchBehavior GetPluginNoMatchBehavior();
void SetNoMatchScene(const OBSWeakSource &);

bool SettingsWindowIsOpened();

} // namespace advss
