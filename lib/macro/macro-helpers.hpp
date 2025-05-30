#pragma once
#include "export-symbol-helper.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <optional>
#include <string_view>
#include <thread>

struct obs_data;
typedef struct obs_data obs_data_t;

namespace advss {

class Macro;
class MacroAction;
class MacroCondition;

EXPORT std::deque<std::shared_ptr<Macro>> &GetMacros();

EXPORT std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroActions(Macro *);
EXPORT std::optional<std::deque<std::shared_ptr<MacroCondition>>>
GetMacroConditions(Macro *);

std::string_view GetSceneSwitchActionId();

constexpr auto macro_func = 10;

EXPORT std::condition_variable &GetMacroWaitCV();
EXPORT std::condition_variable &GetMacroTransitionCV();

EXPORT std::atomic_bool &MacroWaitShouldAbort();
EXPORT void SetMacroAbortWait(bool);

EXPORT bool ShutdownCheckIsNecessary();
EXPORT std::atomic_int &GetShutdownConditionCount();

EXPORT void SetMacroSwitchedScene(bool value);
EXPORT bool MacroSwitchedScene();

EXPORT std::string GetMacroName(const Macro *);

EXPORT std::chrono::high_resolution_clock::time_point
LastMacroConditionCheckTime(const Macro *);

EXPORT bool MacroIsStopped(const Macro *);
EXPORT bool MacroIsPaused(const Macro *);
EXPORT bool
MacroWasPausedSince(const Macro *,
		    const std::chrono::high_resolution_clock::time_point &);
EXPORT bool MacroWasCheckedSinceLastStart(const Macro *);

EXPORT void AddMacroHelperThread(Macro *, std::thread &&);

EXPORT bool CheckMacros();

EXPORT bool RunMacroActions(Macro *);
EXPORT bool RunMacros();
void StopAllMacros();

EXPORT void LoadMacros(obs_data_t *obj);
EXPORT void SaveMacros(obs_data_t *obj);

EXPORT void InvalidateMacroTempVarValues();
EXPORT void ResetMacroConditionTimers(Macro *);
EXPORT void ResetMacroRunCount(Macro *);

bool IsValidMacroSegmentIndex(const Macro *m, const int idx, bool isCondition);

} // namespace advss
