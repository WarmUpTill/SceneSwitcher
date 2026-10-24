#pragma once
#include "export-symbol-helper.hpp"

#include <QString>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <optional>
#include <thread>

struct obs_data;
typedef struct obs_data obs_data_t;
struct obs_data_array;
typedef struct obs_data_array obs_data_array_t;

namespace advss {

class Macro;
class MacroAction;
class MacroCondition;

static const int macro_func = 10;

EXPORT std::deque<std::shared_ptr<Macro>> &GetTopLevelMacros();
std::deque<std::shared_ptr<Macro>> &GetTemporaryMacros();
EXPORT std::deque<std::shared_ptr<Macro>> GetAllMacros();

Macro *GetMacroByName(const char *name);
Macro *GetMacroByQString(const QString &name);
std::weak_ptr<Macro> GetWeakMacroByName(const char *name);

EXPORT std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroActions(Macro *);
EXPORT std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroElseActions(Macro *);
EXPORT std::optional<std::deque<std::shared_ptr<MacroCondition>>>
GetMacroConditions(Macro *);

EXPORT bool IsGroupMacro(Macro *);
// Returns all non-group macros within the group including nested groups
EXPORT std::vector<std::shared_ptr<Macro>> GetGroupMacroEntries(Macro *);
std::vector<std::shared_ptr<Macro>> GetGroupChildren(const Macro *group);
std::vector<std::shared_ptr<Macro>> GetGroupDescendants(const Macro *group);
bool IsMacroInGroup(const Macro *macro, const Macro *group);
// Returns the given macros without the ones contained in one of the given
// groups, sorted by their position in the macro list
std::vector<std::shared_ptr<Macro>>
GetOutermostMacros(const std::vector<std::shared_ptr<Macro>> &macros);
// Returns the given macros and the contents of the given groups, sorted by
// their position in the macro list
std::vector<std::shared_ptr<Macro>>
GetMacrosWithGroupContents(const std::vector<std::shared_ptr<Macro>> &macros);
// Inserts the macro and its group contents at the given position of the macro
// list
void InsertMacroSubtree(int index,
			const std::deque<std::shared_ptr<Macro>> &subtree,
			const std::shared_ptr<Macro> &parent);
// Removes the macro and its group contents from the macro list
void RemoveMacroSubtree(const std::shared_ptr<Macro> &macro);
// Creates a group at the given position of the macro list and moves the given
// macros and their group contents into it
std::shared_ptr<Macro>
InsertGroup(const std::string &name, int index,
	    std::vector<std::shared_ptr<Macro>> &children,
	    const std::shared_ptr<Macro> &parent);

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
EXPORT void SetMacroPaused(Macro *, bool paused);
EXPORT void StopMacro(Macro *);
EXPORT bool
MacroWasPausedSince(const Macro *,
		    const std::chrono::high_resolution_clock::time_point &);
EXPORT bool MacroWasCheckedSinceLastStart(const Macro *);

EXPORT void AddMacroHelperThread(Macro *, std::thread &&);

EXPORT bool CheckMacros();
EXPORT bool CheckMacroConditions(Macro *, bool ignorePause = false);

EXPORT bool RunMacroActions(Macro *, bool forceParallel = false,
			    bool ignorePause = false);
EXPORT bool RunMacroElseActions(Macro *, bool forceParallel = false,
				bool ignorePause = false);
EXPORT bool RunMacros();

void SignalStopAllMacros();
void WaitForAllMacros();

EXPORT void LoadMacros(obs_data_t *obj);
EXPORT void SaveMacros(obs_data_t *obj);
// Appends the exported macros to the macro list.
// Macros for which shouldImport returns false are skipped and their group
// contents are moved to the parent group of the skipped group.
std::vector<std::shared_ptr<Macro>> LoadImportedMacros(
	obs_data_array_t *macros,
	const std::function<bool(const std::shared_ptr<Macro> &)> &shouldImport);

EXPORT void InvalidateMacroTempVarValues();
EXPORT void ResetMacroConditionTimers(Macro *);
EXPORT void ResetMacroRunCount(Macro *);

bool IsValidActionIndex(const Macro *m, const int idx);
bool IsValidElseActionIndex(const Macro *m, const int idx);
bool IsValidConditionIndex(const Macro *m, const int idx);

} // namespace advss
