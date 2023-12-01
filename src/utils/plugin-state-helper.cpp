#include "plugin-state-helper.hpp"
#include "switcher-data.hpp"

namespace advss {

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
	GetSwitcher()->AddIntervalResetStep(step);
}

void StopPlugin()
{
	GetSwitcher()->Stop();
}

void StartPlugin()
{
	GetSwitcher()->Start();
}

void SetPluginNoMatchBehavior(NoMatchBehavior behavior)
{
	GetSwitcher()->switchIfNotMatching = behavior;
}

void SetNoMatchScene(const OBSWeakSource &scene)
{
	GetSwitcher()->nonMatchingScene = scene;
}

NoMatchBehavior GetPluginNoMatchBehavior()
{
	return GetSwitcher()->switchIfNotMatching;
}

bool SettingsWindowIsOpened()
{
	return GetSwitcher()->settingsWindowOpened;
}

} // namespace advss
