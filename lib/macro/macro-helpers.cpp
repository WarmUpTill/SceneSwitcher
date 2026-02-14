#include "macro-helpers.hpp"
#include "macro.hpp"
#include "macro-action-macro.hpp"
#include "plugin-state-helpers.hpp"

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
	if (!macro || !macro->IsGroup()) {
		return {};
	}

	std::vector<std::shared_ptr<Macro>> entries;
	entries.reserve(macro->GroupSize());

	const auto &macros = GetTopLevelMacros();
	for (auto it = macros.begin(); it < macros.end(); it++) {
		if ((*it)->Name() != macro->Name()) {
			continue;
		}
		for (uint32_t i = 1; i <= macro->GroupSize(); i++) {
			entries.emplace_back(*std::next(it, i));
		}
		break;
	}

	return entries;
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

bool RunMacroActions(Macro *macro)
{
	return macro && macro->PerformActions(true);
}

bool RunMacroElseActions(Macro *macro)
{
	return macro && macro->PerformActions(false);
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
