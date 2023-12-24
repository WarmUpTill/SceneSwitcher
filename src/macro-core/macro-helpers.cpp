#include "macro-helpers.hpp"
#include "plugin-state-helpers.hpp"

namespace advss {

static std::atomic_bool abortMacroWait = {false};
static std::atomic_bool macroSceneSwitched = {false};
static std::atomic_int shutdownConditionCount = {0};

std::condition_variable &GetMacroWaitCV()
{
	static std::condition_variable cv;
	return cv;
}

std::condition_variable &GetMacroTransitionCV()
{
	static std::condition_variable cv;
	return cv;
}

std::atomic_bool &MacroWaitShouldAbort()
{
	return abortMacroWait;
}

void SetMacroAbortWait(bool value)
{
	abortMacroWait = value;
}

bool ShutdownCheckIsNecessary()
{
	return shutdownConditionCount > 0;
}

std::atomic_int &GetShutdownConditionCount()
{
	return shutdownConditionCount;
}

void SetMacroSwitchedScene(bool value)
{
	static bool setupDone = false;
	if (!setupDone) {
		// Will always be called with switcher lock already held
		AddIntervalResetStep([]() { macroSceneSwitched = false; },
				     false);
		setupDone = true;
	}
	macroSceneSwitched = value;
}

bool MacroSwitchedScene()
{
	return macroSceneSwitched;
}

} // namespace advss
