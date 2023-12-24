#pragma once
#include <atomic>
#include <condition_variable>
#include <string_view>

namespace advss {

constexpr std::string_view GetSceneSwitchActionId()
{
	return "scene_switch";
}
std::condition_variable &GetMacroWaitCV();
std::condition_variable &GetMacroTransitionCV();
std::atomic_bool &MacroWaitShouldAbort();
void SetMacroAbortWait(bool);
bool ShutdownCheckIsNecessary();
std::atomic_int &GetShutdownConditionCount();
void SetMacroSwitchedScene(bool value);
bool MacroSwitchedScene();

} // namespace advss
