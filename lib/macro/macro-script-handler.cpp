#include "macro-script-handler.hpp"
#include "macro-action-script.hpp"
#include "macro-condition-script.hpp"
#include "plugin-state-helpers.hpp"
#include "log-helper.hpp"
#include "variable.hpp"

#include <obs-properties.h>

namespace advss {

/* Return values */

#define RETURN_STATUS(status)                               \
	{                                                   \
		calldata_set_bool(data, "success", status); \
		return;                                     \
	}
#define RETURN_SUCCESS() RETURN_STATUS(true);
#define RETURN_FAILURE() RETURN_STATUS(false);

/* Script actions */

static constexpr std::string_view nameParam = "name";
static constexpr std::string_view blockingParam = "blocking";
static constexpr std::string_view propertiesParam = "properties";
static constexpr std::string_view defaultSettingsParam = "default_settings";
static constexpr std::string_view triggerSignalParam = "trigger_signal_name";
static constexpr std::string_view registerActionFuncName =
	"advss_register_script_action";
static constexpr std::string_view deregisterActionFuncName =
	"advss_deregister_script_action";

static const std::string registerScriptActionDeclString =
	std::string("bool ") + registerActionFuncName.data() + "(in string " +
	nameParam.data() + ", in bool " + blockingParam.data() + ", in ptr " +
	propertiesParam.data() + ", in ptr " + defaultSettingsParam.data() +
	", out string " + triggerSignalParam.data() + ", out string " +
	GetActionCompletionSignalParamName().data() + ")";
static const std::string deregisterScriptActionDeclString =
	std::string("bool ") + deregisterActionFuncName.data() + "(in string " +
	nameParam.data() + ")";

/* Script conditions */

static constexpr std::string_view changeValueSignalParam =
	"change_value_signal_name";
static constexpr std::string_view registerConditionFuncName =
	"advss_register_script_condition";
static constexpr std::string_view deregisterConditionFuncName =
	"advss_deregister_script_condition";
static const std::string registerScriptConditionDeclString =
	std::string("bool ") + registerConditionFuncName.data() +
	"(in string " + nameParam.data() + ", out string " +
	changeValueSignalParam.data() + ")";
static const std::string deregisterScriptConditionDeclString =
	std::string("bool ") + deregisterConditionFuncName.data() +
	"(in string " + nameParam.data() + ")";

/* Script variables */

static constexpr std::string_view valueParam = "value";
static constexpr std::string_view getVariableValueFuncName =
	"advss_get_variable_value";
static constexpr std::string_view setVariableValueFuncName =
	"advss_set_variable_value";
static const std::string getVariableValueDeclString =
	std::string("bool ") + getVariableValueFuncName.data() + "(in string " +
	nameParam.data() + " out string " + valueParam.data() + ")";
static const std::string setVariableValueDeclString =
	std::string("bool ") + setVariableValueFuncName.data() + "(in string " +
	nameParam.data() + " in string " + valueParam.data() + ")";

static bool setup();
static bool setupDone = setup();
static ScriptHandler *scriptHandler = nullptr;

static bool setup()
{
	AddPluginInitStep([]() { scriptHandler = new ScriptHandler(); });
	AddPluginCleanupStep([]() {
		delete scriptHandler;
		scriptHandler = nullptr;
	});
	return true;
}

ScriptHandler::ScriptHandler()
{
	proc_handler_t *ph = obs_get_proc_handler();
	assert(ph != NULL);

	proc_handler_add(ph, registerScriptActionDeclString.c_str(),
			 &RegisterScriptAction, this);
	proc_handler_add(ph, deregisterScriptActionDeclString.c_str(),
			 &DeregisterScriptAction, this);
	proc_handler_add(ph, registerScriptConditionDeclString.c_str(),
			 &RegisterScriptCondition, this);
	proc_handler_add(ph, deregisterScriptConditionDeclString.c_str(),
			 &DeregisterScriptCondition, this);
	proc_handler_add(ph, getVariableValueDeclString.c_str(),
			 &GetVariableValue, this);
	proc_handler_add(ph, setVariableValueDeclString.c_str(),
			 &SetVariableValue, this);
}

static void replaceWithspace(std::string &string)
{
	std::transform(string.begin(), string.end(), string.begin(), [](char c) {
		return std::isspace(static_cast<unsigned char>(c)) ? '_' : c;
	});
}

static std::string nameToScriptID(const std::string &name)
{
	return std::string("script_") + name;
}

void ScriptHandler::RegisterScriptAction(void *ctx, calldata_t *data)
{
	auto handler = static_cast<ScriptHandler *>(ctx);
	const char *actionName;
	if (!calldata_get_string(data, nameParam.data(), &actionName) ||
	    strlen(actionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerScriptActionDeclString.data(), nameParam.data());
		RETURN_FAILURE();
	}
	bool blocking;
	if (!calldata_get_bool(data, blockingParam.data(), &blocking)) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerActionFuncName.data(), blockingParam.data());
		RETURN_FAILURE();
	}
	obs_properties_t *propertiesPtr = nullptr;
	if (!calldata_get_ptr(data, propertiesParam.data(), &propertiesPtr)) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerActionFuncName.data(), propertiesParam.data());
		RETURN_FAILURE();
	}
	obs_data_t *defaultSettingsPtr = nullptr;
	if (!calldata_get_ptr(data, defaultSettingsParam.data(),
			      &defaultSettingsPtr)) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerActionFuncName.data(),
		     defaultSettingsParam.data());
		RETURN_FAILURE();
	}
	std::lock_guard<std::mutex> lock(handler->_mutex);

	auto properties = std::shared_ptr<obs_properties_t>(
		propertiesPtr, obs_properties_destroy);
	OBSData defaultSettings(defaultSettingsPtr);

	if (handler->_actions.count(actionName) > 0) {
		blog(LOG_WARNING, "[%s] failed! Action \"%s\" already exists!",
		     registerActionFuncName.data(), actionName);
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(actionName);

	std::string signalName = std::string(actionName);
	replaceWithspace(signalName);
	signalName += "_perform_action";
	std::string completionSignalName = signalName + "_complete";

	const auto createScriptAction =
		[id, properties, defaultSettings, blocking, signalName,
		 completionSignalName](
			Macro *m) -> std::shared_ptr<MacroAction> {
		return std::make_shared<MacroActionScript>(
			m, id, properties, defaultSettings, blocking,
			signalName, completionSignalName);
	};
	if (!MacroActionFactory::Register(id, {createScriptAction,
					       MacroActionScriptEdit::Create,
					       actionName})) {
		blog(LOG_WARNING,
		     "[%s] failed! Action id \"%s\" already exists!",
		     registerActionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	blog(LOG_INFO, "[%s] successful for \"%s\"",
	     registerActionFuncName.data(), actionName);

	calldata_set_string(data, triggerSignalParam.data(),
			    signalName.c_str());
	handler->_actions.emplace(
		id, ScriptAction(id, properties, defaultSettings, blocking,
				 signalName, completionSignalName));

	RETURN_SUCCESS();
}

void ScriptHandler::DeregisterScriptAction(void *ctx, calldata_t *data)
{
	auto handler = static_cast<ScriptHandler *>(ctx);
	const char *actionName;
	if (!calldata_get_string(data, nameParam.data(), &actionName) ||
	    strlen(actionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     deregisterActionFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(actionName);
	std::lock_guard<std::mutex> lock(handler->_mutex);

	if (handler->_actions.count(id) == 0) {
		blog(LOG_WARNING,
		     "[%s] failed! Action \"%s\" was never registered!",
		     deregisterActionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	if (!MacroActionFactory::Deregister(id)) {
		blog(LOG_WARNING,
		     "[%s] failed! Action id \"%s\" does not exist!",
		     deregisterActionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	RETURN_SUCCESS();
}

void ScriptHandler::RegisterScriptCondition(void *ctx, calldata_t *data)
{
	auto handler = static_cast<ScriptHandler *>(ctx);
	const char *conditionName;
	if (!calldata_get_string(data, nameParam.data(), &conditionName) ||
	    strlen(conditionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerScriptConditionDeclString.data(),
		     nameParam.data());
		RETURN_FAILURE();
	}

	std::lock_guard<std::mutex> lock(handler->_mutex);

	if (handler->_conditions.count(conditionName) > 0) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition \"%s\" already exists!",
		     registerConditionFuncName.data(), conditionName);
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(conditionName);

	std::string signalName = std::string(conditionName);
	replaceWithspace(signalName);
	signalName += "_change_condition_value";

	const auto createScriptCondition =
		[id, signalName](Macro *m) -> std::shared_ptr<MacroCondition> {
		return std::make_shared<MacroConditionScript>(m, id,
							      signalName);
	};
	if (!MacroConditionFactory::Register(
		    id, {createScriptCondition,
			 MacroConditionScriptEdit::Create, conditionName})) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition id \"%s\" already exists!",
		     registerConditionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	blog(LOG_INFO, "[%s] successful for \"%s\"",
	     registerConditionFuncName.data(), conditionName);

	calldata_set_string(data, changeValueSignalParam.data(),
			    signalName.c_str());
	handler->_conditions.emplace(id, ScriptCondition(id, signalName));

	RETURN_SUCCESS();
}

void ScriptHandler::DeregisterScriptCondition(void *ctx, calldata_t *data)
{
	auto handler = static_cast<ScriptHandler *>(ctx);
	const char *conditionName;
	if (!calldata_get_string(data, nameParam.data(), &conditionName) ||
	    strlen(conditionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     deregisterConditionFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(conditionName);
	std::lock_guard<std::mutex> lock(handler->_mutex);

	if (handler->_conditions.count(id) == 0) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition \"%s\" was never registered!",
		     deregisterConditionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	if (!MacroConditionFactory::Deregister(id)) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition id \"%s\" does not exist!",
		     deregisterConditionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	RETURN_SUCCESS();
}

void ScriptHandler::GetVariableValue(void *, calldata_t *data)
{
	const char *variableName;
	if (!calldata_get_string(data, nameParam.data(), &variableName) ||
	    strlen(variableName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     getVariableValueFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	auto weakVariable = GetWeakVariableByName(variableName);
	auto variable = weakVariable.lock();
	if (!variable) {
		blog(LOG_WARNING,
		     "[%s] failed! \"%s\" variable does not exist!",
		     getVariableValueFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	calldata_set_string(data, valueParam.data(), variable->Value().c_str());
	RETURN_SUCCESS();
}

void ScriptHandler::SetVariableValue(void *, calldata_t *data)
{
	const char *variableName;
	if (!calldata_get_string(data, nameParam.data(), &variableName) ||
	    strlen(variableName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     getVariableValueFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}
	const char *variableValue;
	if (!calldata_get_string(data, valueParam.data(), &variableValue) ||
	    strlen(variableValue) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     getVariableValueFuncName.data(), valueParam.data());
		RETURN_FAILURE();
	}

	auto weakVariable = GetWeakVariableByName(variableName);
	auto variable = weakVariable.lock();
	if (!variable) {
		blog(LOG_WARNING,
		     "[%s] failed! \"%s\" variable does not exist!",
		     getVariableValueFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	variable->SetValue(variableValue);
	RETURN_SUCCESS();
}

static std::string actionSignalNameToSignalDecl(const std::string &name)
{
	return std::string("void ") + name + "()";
}

ScriptAction::ScriptAction(const std::string &id,
			   const std::shared_ptr<obs_properties_t> &properties,
			   const OBSData &defaultSettings, bool blocking,
			   const std::string &signal,
			   const std::string &signalComplete)
	: _id(id),
	  _properties(properties),
	  _defaultSettings(defaultSettings)
{
	signal_handler_add(obs_get_signal_handler(),
			   actionSignalNameToSignalDecl(signal).c_str());
	if (!blocking) {
		return;
	}
	signal_handler_add(
		obs_get_signal_handler(),
		actionSignalNameToSignalDecl(signalComplete).c_str());
}

static std::string conditionSignalNameToSignalDecl(const std::string &name)
{
	return std::string("void ") + name + "(in bool " +
	       GetConditionValueChangeSignalParamName().data() + ")";
}

ScriptCondition::ScriptCondition(const std::string &id,
				 const std::string &signal)
	: _id(id)
{
	signal_handler_add(obs_get_signal_handler(),
			   conditionSignalNameToSignalDecl(signal).c_str());
}

} // namespace advss
