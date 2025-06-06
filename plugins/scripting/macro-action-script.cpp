#include "macro-action-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

namespace advss {

MacroActionScript::MacroActionScript(
	Macro *m, const std::string &id, const OBSData &defaultSettings,
	const std::string &propertiesSignalName,
	const std::string &triggerSignalName,
	const std::string &completionSignalName,
	const std::string &newInstanceSignalName,
	const std::string &deletedInstanceSignalName)
	: MacroAction(m),
	  MacroSegmentScript(defaultSettings, propertiesSignalName,
			     triggerSignalName, completionSignalName,
			     newInstanceSignalName, deletedInstanceSignalName),
	  _id(id)
{
}

MacroActionScript::MacroActionScript(const advss::MacroActionScript &other)
	: MacroAction(other.GetMacro()),
	  MacroSegmentScript(other),
	  _id(other._id)
{
}

bool MacroActionScript::PerformAction()
{
	if (!ScriptHandler::ActionIdIsValid(_id)) {
		blog(LOG_WARNING, "skipping unknown script action \"%s\"",
		     _id.c_str());
		return true;
	}

	(void)SendTriggerSignal();
	return true;
}

void MacroActionScript::LogAction() const
{
	ablog(LOG_INFO, "performing script action \"%s\"", GetId().c_str());
}

bool MacroActionScript::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	MacroSegmentScript::Save(obj);
	return true;
}

bool MacroActionScript::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	MacroSegmentScript::Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionScript::Copy() const
{
	return std::make_shared<MacroActionScript>(*this);
}

void MacroActionScript::WaitForCompletion() const
{
	using namespace std::chrono_literals;
	auto start = std::chrono::high_resolution_clock::now();
	auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
		start - start);
	const auto timeoutMs = GetTimeoutSeconds() * 1000.0;

	std::unique_lock<std::mutex> lock(*GetMutex());
	while (!TriggerIsCompleted()) {
		if (MacroWaitShouldAbort() || MacroIsStopped(GetMacro())) {
			break;
		}

		if ((double)timePassed.count() > timeoutMs) {
			blog(LOG_INFO, "script action timeout (%s)",
			     _id.c_str());
			break;
		}

		GetMacroWaitCV().wait_for(lock, 10ms);
		const auto now = std::chrono::high_resolution_clock::now();
		timePassed =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				now - start);
	}
}

void MacroActionScript::RegisterTempVarHelper(const std::string &variableId,
					      const std::string &name,
					      const std::string &helpText)
{
	AddTempvar(variableId, name, helpText);
}

void MacroActionScript::DeregisterAllTempVarsHelper()
{
	MacroSegment::SetupTempVars();
}

void MacroActionScript::SetTempVarValueHelper(const std::string &variableId,
					      const std::string &value)
{
	MacroAction::SetTempVarValue(variableId, value);
}

void MacroActionScript::SetupTempVars()
{
	// This just exists so MacroSegment::SetupTempVars() is not called.
	// We want the ScriptHandler to handle the registration and clearing of
	// the temp vars.
}

} // namespace advss
