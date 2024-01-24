#include "macro-helpers.hpp"
#include "macro.hpp"
#include "plugin-state-helpers.hpp"

namespace advss {

static std::atomic_bool abortMacroWait = {false};
static std::atomic_bool macroSceneSwitched = {false};
static std::atomic_int shutdownConditionCount = {0};

std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroActions(Macro *macro)
{
	if (!macro) {
		return {};
	}
	return macro->Actions();
}

std::optional<std::deque<std::shared_ptr<MacroCondition>>>
GetMacroConditions(Macro *macro)
{
	if (!macro) {
		return {};
	}
	return macro->Conditions();
}

std::string_view GetSceneSwitchActionId()
{
	return MacroAction::GetDefaultID();
}

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

std::string GetMacroName(Macro *macro)
{
	return macro ? macro->Name() : "";
}

int64_t MillisecondsSinceMacroConditionCheck(Macro *macro)
{
	return macro ? macro->MsSinceLastCheck() : 0;
}

bool MacroIsStopped(Macro *macro)
{
	return macro ? macro->GetStop() : true;
}

bool MacroIsPaused(Macro *macro)
{
	return macro ? macro->Paused() : true;
}

bool MacroWasPausedSince(
	Macro *macro,
	const std::chrono::high_resolution_clock::time_point &time)
{
	return macro ? macro->WasPausedSince(time) : false;
}

void AddMacroHelperThread(Macro *macro, std::thread &&newThread)
{
	if (!macro) {
		return;
	}
	macro->AddHelperThread(std::move(newThread));
}

bool RunMacroActions(Macro *macro)
{
	return macro && macro->PerformActions(true);
}

void ResetMacroConditionTimers(Macro *macro)
{
	if (!macro) {
		return;
	}
	macro->ResetTimers();
}

void ResetMacroRunCount(Macro *macro)
{
	if (!macro) {
		return;
	}
	macro->ResetRunCount();
}

bool IsValidMacroSegmentIndex(Macro *m, const int idx, bool isCondition)
{
	if (!m || idx < 0) {
		return false;
	}
	if (isCondition) {
		if (idx >= (int)m->Conditions().size()) {
			return false;
		}
	} else {
		if (idx >= (int)m->Actions().size()) {
			return false;
		}
	}
	return true;
}

} // namespace advss
