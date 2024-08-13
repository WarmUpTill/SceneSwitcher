#include "macro-script-handler.hpp"
#include "macro-action-script.hpp"
#include "macro-condition-script.hpp"
#include "plugin-state-helpers.hpp"
#include "log-helper.hpp"
#include "variable.hpp"

#include <obs-properties.h>

namespace advss {

std::mutex ScriptHandler::_mutex = {};
std::unordered_map<std::string, ScriptSegmentType> ScriptHandler::_actions = {};
std::unordered_map<std::string, ScriptSegmentType> ScriptHandler::_conditions =
	{};

/* Procedure handler helpers */

#define RETURN_STATUS(status)                               \
	{                                                   \
		calldata_set_bool(data, "success", status); \
		return;                                     \
	}
#define RETURN_SUCCESS() RETURN_STATUS(true);
#define RETURN_FAILURE() RETURN_STATUS(false);

static constexpr std::string_view nameParam = "name";
static constexpr std::string_view defaultSettingsParam = "default_settings";
static constexpr std::string_view propertiesSignalParam =
	"properties_signal_name";
static constexpr std::string_view triggerSignalParam = "trigger_signal_name";

static std::string getRegisterScriptSegmentDeclString(const char *funcName)
{
	return std::string("bool ") + funcName + "(in string " +
	       nameParam.data() + ", in ptr " + defaultSettingsParam.data() +
	       ", out string " + propertiesSignalParam.data() +
	       ", out string " + triggerSignalParam.data() + ")";
}

static std::string getDeregisterScriptSegmentDeclString(const char *funcName)
{
	return std::string("bool ") + funcName + "(in string " +
	       nameParam.data() + ")";
}

/* Script actions */

static constexpr std::string_view registerActionFuncName =
	"advss_register_script_action";
static constexpr std::string_view deregisterActionFuncName =
	"advss_deregister_script_action";

static const std::string registerScriptActionDeclString =
	getRegisterScriptSegmentDeclString(registerActionFuncName.data());
static const std::string deregisterScriptActionDeclString =
	getDeregisterScriptSegmentDeclString(deregisterActionFuncName.data());

/* Script conditions */

static constexpr std::string_view registerConditionFuncName =
	"advss_register_script_condition";
static constexpr std::string_view deregisterConditionFuncName =
	"advss_deregister_script_condition";

static const std::string registerScriptConditionDeclString =
	getRegisterScriptSegmentDeclString(registerConditionFuncName.data());
static const std::string deregisterScriptConditionDeclString =
	getDeregisterScriptSegmentDeclString(
		deregisterConditionFuncName.data());

/* Script variables */

static constexpr std::string_view valueParam = "value";
static constexpr std::string_view getVariableValueFuncName =
	"advss_get_variable_value";
static constexpr std::string_view setVariableValueFuncName =
	"advss_set_variable_value";
static const std::string getVariableValueDeclString =
	std::string("bool ") + getVariableValueFuncName.data() + "(in string " +
	nameParam.data() + ", out string " + valueParam.data() + ")";
static const std::string setVariableValueDeclString =
	std::string("bool ") + setVariableValueFuncName.data() + "(in string " +
	nameParam.data() + ", in string " + valueParam.data() + ")";

static bool setup();
static bool setupDone = setup();

static bool setup()
{
	proc_handler_t *ph = obs_get_proc_handler();
	assert(ph != NULL);

	proc_handler_add(ph, registerScriptActionDeclString.c_str(),
			 &ScriptHandler::RegisterScriptAction, nullptr);
	proc_handler_add(ph, deregisterScriptActionDeclString.c_str(),
			 &ScriptHandler::DeregisterScriptAction, nullptr);
	proc_handler_add(ph, registerScriptConditionDeclString.c_str(),
			 &ScriptHandler::RegisterScriptCondition, nullptr);
	proc_handler_add(ph, deregisterScriptConditionDeclString.c_str(),
			 &ScriptHandler::DeregisterScriptCondition, nullptr);
	proc_handler_add(ph, getVariableValueDeclString.c_str(),
			 &ScriptHandler::GetVariableValue, nullptr);
	proc_handler_add(ph, setVariableValueDeclString.c_str(),
			 &ScriptHandler::SetVariableValue, nullptr);
	return true;
}

static void replaceWhitespace(std::string &string)
{
	std::transform(string.begin(), string.end(), string.begin(), [](char c) {
		return std::isspace(static_cast<unsigned char>(c)) ? '_' : c;
	});
}

static std::string nameToScriptID(const std::string &name)
{
	return std::string("script_") + name;
}

static std::string getTriggerSignal(const std::string &name,
				    const bool isAction)
{
	std::string signal = name;
	replaceWhitespace(signal);
	signal += "_run";
	signal += isAction ? "_action" : "_condition";
	return signal;
}

static std::string getCompletionSignal(const std::string &name,
				       const bool isAction)
{
	auto signal = getTriggerSignal(name, isAction);
	signal += "_complete";
	return signal;
}

static std::string getPropertiesSignal(const std::string &name,
				       const bool isAction)
{
	std::string signal = name;
	replaceWhitespace(signal);
	signal += isAction ? "_action" : "_condition";
	signal += "_get_properties";
	return signal;
}

void ScriptHandler::RegisterScriptAction(void *, calldata_t *data)
{
	const char *actionName;
	if (!calldata_get_string(data, nameParam.data(), &actionName) ||
	    strlen(actionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerScriptActionDeclString.data(), nameParam.data());
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
	std::lock_guard<std::mutex> lock(_mutex);
	OBSData defaultSettings(defaultSettingsPtr);
	obs_data_release(defaultSettingsPtr);

	if (_actions.count(actionName) > 0) {
		blog(LOG_WARNING, "[%s] failed! Action \"%s\" already exists!",
		     registerActionFuncName.data(), actionName);
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(actionName);
	auto triggerSignalName = getTriggerSignal(actionName, true);
	auto completionSignalName = getCompletionSignal(actionName, true);
	auto propertiesSignalName = getPropertiesSignal(actionName, true);

	const auto createScriptAction =
		[id, defaultSettings, propertiesSignalName, triggerSignalName,
		 completionSignalName](
			Macro *m) -> std::shared_ptr<MacroAction> {
		return std::make_shared<MacroActionScript>(
			m, id, defaultSettings, propertiesSignalName,
			triggerSignalName, completionSignalName);
	};
	if (!MacroActionFactory::Register(id, {createScriptAction,
					       MacroSegmentScriptEdit::Create,
					       actionName})) {
		blog(LOG_WARNING,
		     "[%s] failed! Action id \"%s\" already exists!",
		     registerActionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	blog(LOG_INFO, "[%s] successful for \"%s\"",
	     registerActionFuncName.data(), actionName);

	calldata_set_string(data, triggerSignalParam.data(),
			    triggerSignalName.c_str());
	calldata_set_string(data, propertiesSignalParam.data(),
			    propertiesSignalName.c_str());
	_actions.emplace(id, ScriptSegmentType(id, propertiesSignalName,
					       triggerSignalName,
					       completionSignalName));

	RETURN_SUCCESS();
}

void ScriptHandler::DeregisterScriptAction(void *, calldata_t *data)
{
	const char *actionName;
	if (!calldata_get_string(data, nameParam.data(), &actionName) ||
	    strlen(actionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     deregisterActionFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(actionName);
	std::lock_guard<std::mutex> lock(_mutex);

	if (_actions.count(id) == 0) {
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

	auto it = _actions.find(id);
	if (it != _actions.end()) {
		_actions.erase(it);
	}

	RETURN_SUCCESS();
}

void ScriptHandler::RegisterScriptCondition(void *, calldata_t *data)
{
	const char *conditionName;
	if (!calldata_get_string(data, nameParam.data(), &conditionName) ||
	    strlen(conditionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerScriptConditionDeclString.data(),
		     nameParam.data());
		RETURN_FAILURE();
	}

	obs_data_t *defaultSettingsPtr = nullptr;
	if (!calldata_get_ptr(data, defaultSettingsParam.data(),
			      &defaultSettingsPtr)) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     registerScriptConditionDeclString.data(),
		     defaultSettingsParam.data());
		RETURN_FAILURE();
	}

	std::lock_guard<std::mutex> lock(_mutex);
	OBSData defaultSettings(defaultSettingsPtr);
	obs_data_release(defaultSettingsPtr);

	if (_conditions.count(conditionName) > 0) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition \"%s\" already exists!",
		     registerConditionFuncName.data(), conditionName);
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(conditionName);
	auto triggerSignalName = getTriggerSignal(conditionName, false);
	auto completionSignalName = getCompletionSignal(conditionName, false);
	auto propertiesSignalName = getPropertiesSignal(conditionName, false);

	const auto createScriptCondition =
		[id, defaultSettings, propertiesSignalName, triggerSignalName,
		 completionSignalName](
			Macro *m) -> std::shared_ptr<MacroCondition> {
		return std::make_shared<MacroConditionScript>(
			m, id, defaultSettings, propertiesSignalName,
			triggerSignalName, completionSignalName);
	};
	if (!MacroConditionFactory::Register(
		    id, {createScriptCondition, MacroSegmentScriptEdit::Create,
			 conditionName})) {
		blog(LOG_WARNING,
		     "[%s] failed! Condition id \"%s\" already exists!",
		     registerConditionFuncName.data(), id.c_str());
		RETURN_FAILURE();
	}

	blog(LOG_INFO, "[%s] successful for \"%s\"",
	     registerConditionFuncName.data(), conditionName);

	calldata_set_string(data, triggerSignalParam.data(),
			    triggerSignalName.c_str());
	calldata_set_string(data, propertiesSignalParam.data(),
			    propertiesSignalName.c_str());
	_conditions.emplace(id, ScriptSegmentType(id, propertiesSignalName,
						  triggerSignalName,
						  completionSignalName));

	RETURN_SUCCESS();
}

void ScriptHandler::DeregisterScriptCondition(void *, calldata_t *data)
{
	const char *conditionName;
	if (!calldata_get_string(data, nameParam.data(), &conditionName) ||
	    strlen(conditionName) == 0) {
		blog(LOG_WARNING, "[%s] failed! \"%s\" parameter missing!",
		     deregisterConditionFuncName.data(), nameParam.data());
		RETURN_FAILURE();
	}

	const std::string id = nameToScriptID(conditionName);
	std::lock_guard<std::mutex> lock(_mutex);

	if (_conditions.count(id) == 0) {
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

	auto it = _conditions.find(id);
	if (it != _conditions.end()) {
		_conditions.erase(it);
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

bool ScriptHandler::ActionIdIsValid(const std::string &id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _actions.count(id) > 0;
}

bool ScriptHandler::ConditionIdIsValid(const std::string &id)
{
	std::lock_guard<std::mutex> lock(_mutex);
	return _conditions.count(id) > 0;
}

static std::string signalNameToTriggerSignalDecl(const std::string &name)
{
	return std::string("void ") + name + "()";
}

static std::string signalNameToPropertiesSignalDecl(const std::string &name)
{
	return std::string("void ") + name + "(in ptr " +
	       GetPropertiesSignalParamName().data() + ")";
}
static std::string signalNameToCompletionSignalDecl(const std::string &name)
{
	return std::string("bool ") + name + "(in int " +
	       GeCompletionIdParamName().data() + ")";
}

ScriptSegmentType::ScriptSegmentType(const std::string &id,
				     const std::string &propertiesSignal,
				     const std::string &triggerSignal,
				     const std::string &completionSignal)
	: _id(id)
{
	signal_handler_add(
		obs_get_signal_handler(),
		signalNameToPropertiesSignalDecl(propertiesSignal).c_str());
	signal_handler_add(
		obs_get_signal_handler(),
		signalNameToTriggerSignalDecl(triggerSignal).c_str());
	signal_handler_add(
		obs_get_signal_handler(),
		signalNameToCompletionSignalDecl(completionSignal).c_str());
}

} // namespace advss
