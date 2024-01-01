#pragma once
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

std::deque<std::shared_ptr<Macro>> &GetMacros();

std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroActions(Macro *);
std::optional<std::deque<std::shared_ptr<MacroCondition>>>
GetMacroConditions(Macro *);

constexpr std::string_view GetSceneSwitchActionId()
{
	return "scene_switch";
}

constexpr auto macro_func = 10;

std::condition_variable &GetMacroWaitCV();
std::condition_variable &GetMacroTransitionCV();

std::atomic_bool &MacroWaitShouldAbort();
void SetMacroAbortWait(bool);

bool ShutdownCheckIsNecessary();
std::atomic_int &GetShutdownConditionCount();

void SetMacroSwitchedScene(bool value);
bool MacroSwitchedScene();

std::string GetMacroName(Macro *);

int64_t MillisecondsSinceMacroConditionCheck(Macro *);

bool MacroIsStopped(Macro *);
bool MacroIsPaused(Macro *);

void AddMacroHelperThread(Macro *, std::thread &&);

bool CheckMacros();

bool RunMacroActions(Macro *);
bool RunMacros();

void LoadMacros(obs_data_t *obj);
void SaveMacros(obs_data_t *obj);

void InvalidateMacroTempVarValues();
void ResetMacroConditionTimers(Macro *);
void ResetMacroRunCount(Macro *);

} // namespace advss
