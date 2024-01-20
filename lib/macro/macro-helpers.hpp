#pragma once
#include "export-symbol-helper.hpp"

#include <atomic>
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

EXPORT std::string GetMacroName(Macro *);

EXPORT int64_t MillisecondsSinceMacroConditionCheck(Macro *);

EXPORT bool MacroIsStopped(Macro *);
EXPORT bool MacroIsPaused(Macro *);

EXPORT void AddMacroHelperThread(Macro *, std::thread &&);

EXPORT bool CheckMacros();

EXPORT bool RunMacroActions(Macro *);
EXPORT bool RunMacros();

EXPORT void LoadMacros(obs_data_t *obj);
EXPORT void SaveMacros(obs_data_t *obj);

EXPORT void InvalidateMacroTempVarValues();
EXPORT void ResetMacroConditionTimers(Macro *);
EXPORT void ResetMacroRunCount(Macro *);

bool IsValidMacroSegmentIndex(Macro *m, const int idx, bool isCondition);

} // namespace advss
