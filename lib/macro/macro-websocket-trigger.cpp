#include "websocket-api.hpp"
#include "log-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "variable.hpp"
#include "macro-helpers.hpp"

#include <obs.hpp>

namespace advss {

static bool setup();
static bool setupDone = setup();
static void receiveRunMacroMessage(obs_data_t *data, obs_data_t *);
static void receiveSetVariablesMessage(obs_data_t *data, obs_data_t *);

static const char *runMacroMessage = "AdvancedSceneSwitcherRunMacro";
static const char *setVariablesMessage = "AdvancedSceneSwitcherSetVariables";

static bool setup()
{
	AddPluginInitStep([]() {
		RegisterWebsocketRequest(runMacroMessage,
					 receiveRunMacroMessage);
		RegisterWebsocketRequest(setVariablesMessage,
					 receiveSetVariablesMessage);
	});
	return true;
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

static void receiveRunMacroMessage(obs_data_t *data, obs_data_t *)
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
	if (runElse) {
		RunMacroElseActions(macro.get());
	} else {
		RunMacroActions(macro.get());
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
