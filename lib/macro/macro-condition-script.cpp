#include "macro-condition-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

namespace advss {

MacroConditionScript::MacroConditionScript(
	Macro *m, const std::string &id, const OBSData &defaultSettings,
	const std::string &propertiesSignalName,
	const std::string &triggerSignal, const std::string &completionSignal)
	: MacroCondition(m),
	  MacroSegmentScript(defaultSettings, propertiesSignalName,
			     triggerSignal, completionSignal),
	  _id(id)
{
}

MacroConditionScript::MacroConditionScript(
	const advss::MacroConditionScript &other)
	: MacroCondition(other.GetMacro()),
	  MacroSegmentScript(other),
	  _id(other._id)
{
}

bool MacroConditionScript::CheckCondition()
{
	if (!ScriptHandler::ConditionIdIsValid(_id)) {
		blog(LOG_WARNING, "skipping unknown script condition \"%s\"",
		     _id.c_str());
		return false;
	}

	return SendTriggerSignal();
}

bool MacroConditionScript::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	MacroSegmentScript::Save(obj);
	return true;
}

bool MacroConditionScript::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	MacroSegmentScript::Load(obj);
	return true;
}

void MacroConditionScript::WaitForCompletion() const
{
	using namespace std::chrono_literals;
	auto start = std::chrono::high_resolution_clock::now();
	auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
		start - start);
	const auto timeoutMs = GetTimeoutSeconds() * 1000.0;

	while (!TriggerIsCompleted()) {
		if (MacroWaitShouldAbort() || MacroIsStopped(GetMacro())) {
			break;
		}

		if ((double)timePassed.count() > timeoutMs) {
			blog(LOG_INFO, "script condition timeout (%s)",
			     _id.c_str());
			break;
		}

		std::this_thread::sleep_for(10ms);
		const auto now = std::chrono::high_resolution_clock::now();
		timePassed =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				now - start);
	}
}

} // namespace advss
