#include "macro-helpers.hpp"
#include "macro.hpp"
#include "macro-action-macro.hpp"
#include "plugin-state-helpers.hpp"

#include <algorithm>

namespace advss {

static std::atomic_bool abortMacroWait = {false};
static std::atomic_bool macroSceneSwitched = {false};
static std::atomic_int shutdownConditionCount = {0};

static void appendNestedMacros(std::deque<std::shared_ptr<Macro>> &macros,
			       Macro *macro)
{
	if (!macro) {
		return;
	}

	const auto iterate = [&macros](const std::deque<
				       std::shared_ptr<MacroAction>> &actions) {
		for (const auto &action : actions) {
			const auto nestedMacroAction =
				dynamic_cast<MacroActionMacro *>(action.get());
			if (!nestedMacroAction) {
				continue;
			}

			macros.push_back(nestedMacroAction->_nestedMacro);
			appendNestedMacros(
				macros, nestedMacroAction->_nestedMacro.get());
		}
	};

	iterate(macro->Actions());
	iterate(macro->ElseActions());
}

std::deque<std::shared_ptr<Macro>> &GetTopLevelMacros()
{
	static std::deque<std::shared_ptr<Macro>> macros;
	return macros;
}

std::deque<std::shared_ptr<Macro>> &GetTemporaryMacros()
{
	static std::deque<std::shared_ptr<Macro>> tempMacros;
	return tempMacros;
}

std::deque<std::shared_ptr<Macro>> GetAllMacros()
{
	auto macros = GetTopLevelMacros();
	for (const auto &topLevelMacro : macros) {
		appendNestedMacros(macros, topLevelMacro.get());
	}

	const auto &tempMacros = GetTemporaryMacros();
	macros.insert(macros.end(), tempMacros.begin(), tempMacros.end());
	for (const auto &tempMacro : tempMacros) {
		appendNestedMacros(macros, tempMacro.get());
	}

	return macros;
}

Macro *GetMacroByName(const char *name)
{
	for (const auto &m : GetTopLevelMacros()) {
		if (m->Name() == name) {
			return m.get();
		}
	}

	return nullptr;
}

Macro *GetMacroByQString(const QString &name)
{
	return GetMacroByName(name.toUtf8().constData());
}

std::weak_ptr<Macro> GetWeakMacroByName(const char *name)
{
	for (const auto &m : GetTopLevelMacros()) {
		if (m->Name() == name) {
			return m;
		}
	}

	return {};
}

std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroActions(Macro *macro)
{
	if (!macro) {
		return {};
	}
	return macro->Actions();
}

std::optional<std::deque<std::shared_ptr<MacroAction>>>
GetMacroElseActions(Macro *macro)
{
	if (!macro) {
		return {};
	}
	return macro->ElseActions();
}

std::optional<std::deque<std::shared_ptr<MacroCondition>>>
GetMacroConditions(Macro *macro)
{
	if (!macro) {
		return {};
	}
	return macro->Conditions();
}

bool IsGroupMacro(Macro *macro)
{
	return macro && macro->IsGroup();
}

std::vector<std::shared_ptr<Macro>> GetGroupMacroEntries(Macro *macro)
{
	auto entries = GetGroupDescendants(macro);
	entries.erase(std::remove_if(entries.begin(), entries.end(),
				     [](const std::shared_ptr<Macro> &entry) {
					     return entry->IsGroup();
				     }),
		      entries.end());
	return entries;
}

std::vector<std::shared_ptr<Macro>> GetGroupChildren(const Macro *group)
{
	if (!group || !group->IsGroup()) {
		return {};
	}

	std::vector<std::shared_ptr<Macro>> children;
	for (const auto &macro : GetTopLevelMacros()) {
		if (macro->Parent().get() == group) {
			children.emplace_back(macro);
		}
	}
	return children;
}

std::vector<std::shared_ptr<Macro>> GetGroupDescendants(const Macro *group)
{
	if (!group || !group->IsGroup()) {
		return {};
	}

	std::vector<std::shared_ptr<Macro>> descendants;
	descendants.reserve(group->GroupSize());
	for (const auto &macro : GetTopLevelMacros()) {
		if (IsMacroInGroup(macro.get(), group)) {
			descendants.emplace_back(macro);
		}
	}
	return descendants;
}

bool IsMacroInGroup(const Macro *macro, const Macro *group)
{
	if (!macro || !group) {
		return false;
	}

	for (auto parent = macro->Parent(); parent; parent = parent->Parent()) {
		if (parent.get() == group) {
			return true;
		}
	}
	return false;
}

std::vector<std::shared_ptr<Macro>>
GetOutermostMacros(const std::vector<std::shared_ptr<Macro>> &macros)
{
	std::vector<std::shared_ptr<Macro>> result;
	for (const auto &macro : GetTopLevelMacros()) {
		const bool isGiven = std::find(macros.begin(), macros.end(),
					       macro) != macros.end();
		const bool isInGivenGroup = std::any_of(
			macros.begin(), macros.end(),
			[&macro](const std::shared_ptr<Macro> &group) {
				return IsMacroInGroup(macro.get(), group.get());
			});
		if (isGiven && !isInGivenGroup) {
			result.emplace_back(macro);
		}
	}
	return result;
}

std::vector<std::shared_ptr<Macro>>
GetMacrosWithGroupContents(const std::vector<std::shared_ptr<Macro>> &macros)
{
	std::vector<std::shared_ptr<Macro>> result;
	for (const auto &macro : GetTopLevelMacros()) {
		const bool include = std::any_of(
			macros.begin(), macros.end(),
			[&macro](const std::shared_ptr<Macro> &given) {
				return given == macro ||
				       IsMacroInGroup(macro.get(), given.get());
			});
		if (include) {
			result.emplace_back(macro);
		}
	}
	return result;
}

static std::deque<std::shared_ptr<Macro>>::iterator
subtreeEnd(std::deque<std::shared_ptr<Macro>> &macros,
	   std::deque<std::shared_ptr<Macro>>::iterator it)
{
	const auto remaining = std::distance(it, macros.end());
	const auto size =
		std::min<std::ptrdiff_t>(1 + (*it)->GroupSize(), remaining);
	return std::next(it, size);
}

void InsertMacroSubtree(int index,
			const std::deque<std::shared_ptr<Macro>> &subtree,
			const std::shared_ptr<Macro> &parent)
{
	if (subtree.empty()) {
		return;
	}

	auto &macros = GetTopLevelMacros();
	Macro::PrepareMoveToGroup(parent, subtree.front());
	index = std::clamp(index, 0, (int)macros.size());
	macros.insert(macros.begin() + index, subtree.begin(), subtree.end());
}

void RemoveMacroSubtree(const std::shared_ptr<Macro> &macro)
{
	auto &macros = GetTopLevelMacros();
	auto it = std::find(macros.begin(), macros.end(), macro);
	if (it == macros.end()) {
		return;
	}

	const auto end = subtreeEnd(macros, it);
	Macro::PrepareMoveToGroup(nullptr, macro);
	macros.erase(it, end);
}

std::shared_ptr<Macro>
InsertGroup(const std::string &name, int index,
	    std::vector<std::shared_ptr<Macro>> &children,
	    const std::shared_ptr<Macro> &parent)
{
	auto group = Macro::CreateGroup(name, children);
	if (children.empty()) {
		Macro::PrepareMoveToGroup(parent, group);
	}

	auto &macros = GetTopLevelMacros();
	index = std::clamp(index, 0, (int)macros.size());
	macros.insert(macros.begin() + index, group);

	auto previous = group;
	for (const auto &child : children) {
		auto start = std::find(macros.begin(), macros.end(), child);
		if (start == macros.end()) {
			continue;
		}
		const auto end = subtreeEnd(macros, start);
		const std::vector<std::shared_ptr<Macro>> subtree(start, end);
		macros.erase(start, end);

		auto position =
			std::find(macros.begin(), macros.end(), previous);
		position = previous == group ? std::next(position)
					     : subtreeEnd(macros, position);
		macros.insert(position, subtree.begin(), subtree.end());
		previous = child;
	}
	return group;
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
		AddIntervalResetStep([]() { macroSceneSwitched = false; });
		setupDone = true;
	}
	macroSceneSwitched = value;
}

bool MacroSwitchedScene()
{
	return macroSceneSwitched;
}

std::string GetMacroName(const Macro *macro)
{
	return macro ? macro->Name() : "";
}

std::chrono::high_resolution_clock::time_point
LastMacroConditionCheckTime(const Macro *macro)
{
	return macro ? macro->LastConditionCheckTime()
		     : std::chrono::high_resolution_clock::time_point{};
}

bool MacroIsStopped(const Macro *macro)
{
	return macro ? macro->GetStop() : true;
}

bool MacroIsPaused(const Macro *macro)
{
	return macro ? macro->Paused() : true;
}

void SetMacroPaused(Macro *macro, bool paused)
{
	if (macro) {
		macro->SetPaused(paused);
	}
}

void StopMacro(Macro *macro)
{
	if (macro) {
		macro->Stop();
	}
}

bool MacroWasPausedSince(
	const Macro *macro,
	const std::chrono::high_resolution_clock::time_point &time)
{
	return macro ? macro->WasPausedSince(time) : false;
}

bool MacroWasCheckedSinceLastStart(const Macro *macro)
{
	if (!macro) {
		return false;
	}
	return macro->LastConditionCheckTime().time_since_epoch().count() != 0;
}

void AddMacroHelperThread(Macro *macro, std::thread &&newThread)
{
	if (!macro) {
		return;
	}
	macro->AddHelperThread(std::move(newThread));
}

bool RunMacroActions(Macro *macro, bool forceParallel, bool ignorePause)
{
	return macro && macro->PerformActions(true, forceParallel, ignorePause);
}

bool RunMacroElseActions(Macro *macro, bool forceParallel, bool ignorePause)
{
	return macro &&
	       macro->PerformActions(false, forceParallel, ignorePause);
}

bool CheckMacroConditions(Macro *macro, bool ignorePause)
{
	return macro && macro->CheckConditions(ignorePause);
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

bool IsValidActionIndex(const Macro *m, const int idx)
{
	if (!m || idx < 0) {
		return false;
	}

	if (idx >= (int)m->Actions().size()) {
		return false;
	}

	return true;
}

bool IsValidElseActionIndex(const Macro *m, const int idx)
{
	if (!m || idx < 0) {
		return false;
	}

	if (idx >= (int)m->ElseActions().size()) {
		return false;
	}

	return true;
}

bool IsValidConditionIndex(const Macro *m, const int idx)
{
	if (!m || idx < 0) {
		return false;
	}

	if (idx >= (int)m->Conditions().size()) {
		return false;
	}

	return true;
}

} // namespace advss
