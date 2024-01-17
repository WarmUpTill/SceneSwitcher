#include "plugin-state-helpers.hpp"
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

void AddIntervalResetStep(std::function<void()> step, bool lock)
{
	GetSwitcher()->AddIntervalResetStep(step, lock);
}

void AddPluginInitStep(std::function<void()> step)
{
	GetSwitcher()->AddPluginInitStep(step);
}

void AddPluginPostLoadStep(std::function<void()> step)
{
	GetSwitcher()->AddPluginPostLoadStep(step);
}

void AddPluginCleanupStep(std::function<void()> step)
{
	GetSwitcher()->AddPluginCleanupStep(step);
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
	return GetSwitcher()->obsIsShuttingDown;
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

} // namespace advss
