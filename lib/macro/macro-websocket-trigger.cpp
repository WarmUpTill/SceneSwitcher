#include "websocket-api.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "variable.hpp"
#include "macro-helpers.hpp"
#include "macro-settings.hpp"
#include "macro.hpp"

#include <obs.hpp>

#include <mutex>
#include <unordered_map>

namespace advss {

static bool setup();
static bool setupDone = setup();
static void receiveRunMacroMessage(obs_data_t *data, obs_data_t *response);
static void receiveSetVariablesMessage(obs_data_t *data, obs_data_t *);
static void sendMacroFinishedEvent(const Macro &macro,
				   const Macro::RunResult &result, bool match);

static const char *runMacroMessage = "AdvancedSceneSwitcherRunMacro";
static const char *setVariablesMessage = "AdvancedSceneSwitcherSetVariables";
static const char *macroFinishedEvent = "AdvancedSceneSwitcherMacroFinished";

static std::mutex clientRunIdsMutex;
static std::unordered_map<uint64_t, std::string> clientRunIds;

static bool setup()
{
	AddPluginInitStep([]() {
		RegisterWebsocketRequest(runMacroMessage,
					 receiveRunMacroMessage);
		RegisterWebsocketRequest(setVariablesMessage,
					 receiveSetVariablesMessage);
		AddMacroRunFinishedCallback(sendMacroFinishedEvent);
	});
	return true;
}

static void addClientRunId(uint64_t runId, const std::string &clientRunId)
{
	std::lock_guard<std::mutex> lock(clientRunIdsMutex);
	clientRunIds[runId] = clientRunId;
}

static std::string takeClientRunId(uint64_t runId)
{
	std::lock_guard<std::mutex> lock(clientRunIdsMutex);
	auto it = clientRunIds.find(runId);
	if (it == clientRunIds.end()) {
		return "";
	}
	auto clientRunId = it->second;
	clientRunIds.erase(it);
	return clientRunId;
}

static void sendMacroFinishedEvent(const Macro &macro,
				   const Macro::RunResult &result, bool match)
{
	const auto clientRunId = takeClientRunId(result.id);
	if (macro.IsNested() ||
	    !GetGlobalMacroSettings()._sendMacroFinishedEvent) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "name", macro.Name().c_str());
	obs_data_set_int(data, "runId", static_cast<long long>(result.id));
	if (!clientRunId.empty()) {
		obs_data_set_string(data, "clientRunId", clientRunId.c_str());
	}
	obs_data_set_bool(data, "elseActions", !match);
	obs_data_set_string(data, "status", RunStatusToString(result.status));
	SendWebsocketVendorEvent(macroFinishedEvent, data);
}

static void setVariables(obs_data_t *data)
{
	OBSDataArrayAutoRelease variables =
		obs_data_get_array(data, "variables");
	size_t count = obs_data_array_count(variables);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease item = obs_data_array_item(variables, i);
		if (!obs_data_has_user_value(item, "name")) {
			blog(LOG_INFO,
			     "ignoring invalid variable entry in \"%s\" websocket message (missing \"name\")",
			     runMacroMessage);
			continue;
		}

		if (!obs_data_has_user_value(item, "value")) {
			blog(LOG_INFO,
			     "ignoring invalid variable entry in \"%s\" websocket message (missing \"value\")",
			     runMacroMessage);
			continue;
		}

		auto varName = obs_data_get_string(item, "name");
		auto value = obs_data_get_string(item, "value");

		auto weakVar = GetWeakVariableByName(varName);
		auto var = weakVar.lock();
		if (!var) {
			blog(LOG_INFO,
			     "ignoring invalid variable name \"%s\" in \"%s\" websocket message",
			     varName, runMacroMessage);
			continue;
		}

		var->SetValue(value);
	}
}

static void receiveRunMacroMessage(obs_data_t *data, obs_data_t *response)
{
	if (!obs_data_has_user_value(data, "name")) {
		blog(LOG_INFO,
		     "ignoring invalid \"%s\" websocket message (missing \"name\")",
		     runMacroMessage);
		return;
	}

	auto name = obs_data_get_string(data, "name");
	const auto weakMacro =
		GetWeakMacroByName(obs_data_get_string(data, "name"));
	const auto macro = weakMacro.lock();
	if (!macro) {
		blog(LOG_INFO,
		     "ignoring invalid \"%s\" websocket message (macro \"%s\") not found",
		     runMacroMessage, name);
		return;
	}

	if (obs_data_has_user_value(data, "variables")) {
		setVariables(data);
	}

	const bool runElse = obs_data_get_bool(data, "runElseActions");
	const std::string clientRunId =
		obs_data_get_string(data, "clientRunId");
	const auto runId = NewMacroRunId();
	if (!clientRunId.empty()) {
		addClientRunId(runId, clientRunId);
	}
	const auto result =
		macro->PerformActionsWithStatus(!runElse, false, false, runId);
	obs_data_set_string(response, "status",
			    RunStatusToString(result.status));
	obs_data_set_int(response, "runId", static_cast<long long>(result.id));
	if (!clientRunId.empty()) {
		obs_data_set_string(response, "clientRunId",
				    clientRunId.c_str());
	}
}

static void receiveSetVariablesMessage(obs_data_t *data, obs_data_t *)
{
	if (!obs_data_has_user_value(data, "variables")) {
		blog(LOG_INFO,
		     "ignoring invalid \"%s\" websocket message (missing \"variables\")",
		     setVariablesMessage);
		return;
	}

	setVariables(data);
}

} // namespace advss
