#include "plugin-state-helpers.hpp"

namespace advss {

namespace {

std::string g_foregroundWindowTitle;
std::string g_previousForegroundWindowTitle;

} // namespace

void SetStubForegroundWindowTitle(const std::string &title)
{
	g_foregroundWindowTitle = title;
}

void SetStubPreviousForegroundWindowTitle(const std::string &title)
{
	g_previousForegroundWindowTitle = title;
}

void SavePluginSettings(obs_data_t *) {}
void LoadPluginSettings(obs_data_t *) {}
void AddEarlySaveStep(std::function<void(obs_data_t *)>) {}
void AddEarlyLoadStep(std::function<void(obs_data_t *)>) {}
void AddSaveStep(std::function<void(obs_data_t *)>) {}
void AddLoadStep(std::function<void(obs_data_t *)>) {}
void AddPostLoadStep(std::function<void()>) {}
void AddIntervalResetStep(std::function<void()>) {}
void RunSaveSteps(obs_data_t *) {}
void RunLoadSteps(obs_data_t *) {}
void RunAndClearPostLoadSteps() {}
void ClearPostLoadSteps() {}

void AddPluginInitStep(std::function<void()>) {}
void AddPluginPostLoadStep(std::function<void()>) {}
void AddPluginCleanupStep(std::function<void()>) {}
void RunPluginInitSteps() {}
void RunPluginPostLoadSteps() {}
void RunPluginCleanupSteps() {}

void StopPlugin() {}
void StartPlugin() {}
bool PluginIsRunning()
{
	return false;
}
int GetIntervalValue()
{
	return 0;
}
void AddStartStep(std::function<void()>) {}
void AddStopStep(std::function<void()>) {}
void RunStartSteps() {}
void RunStopSteps() {}
void RunIntervalResetSteps() {}

void SetPluginNoMatchBehavior(NoMatchBehavior) {}
NoMatchBehavior GetPluginNoMatchBehavior()
{
	return NoMatchBehavior::NO_SWITCH;
}
void SetNoMatchScene(const OBSWeakSource &) {}

std::string ForegroundWindowTitle()
{
	return g_foregroundWindowTitle;
}
std::string PreviousForegroundWindowTitle()
{
	return g_previousForegroundWindowTitle;
}
bool SettingsWindowIsOpened()
{
	return false;
}
bool HighlightUIElementsEnabled()
{
	return false;
}

bool OBSIsShuttingDown()
{
	return false;
}
bool InitialLoadIsComplete()
{
	return false;
}
bool IsFirstInterval()
{
	return false;
}
bool IsFirstIntervalAfterStop()
{
	return false;
}

void SetMacroHighlightingEnabled(bool) {}
bool IsMacroHighlightingEnabled()
{
	return false;
}

} // namespace advss
