#pragma once
#include <obs.hpp>
#include <mutex>
#include <string>
#include <unordered_map>

namespace advss {

class ScriptAction {
public:
	ScriptAction() = delete;
	ScriptAction(const std::string &id, const OBSData &defaultSettings,
		     const std::string &propertiesSignal,
		     const std::string &triggerSignal,
		     const std::string &completionSignal);

private:
	std::shared_ptr<obs_properties_t> _properties;
	OBSData _defaultSettings;
	std::string _id;
};

class ScriptCondition {
public:
	ScriptCondition() = delete;
	ScriptCondition(const std::string &id, const std::string &signal);

private:
	std::string _id;
};

class ScriptHandler {
public:
	ScriptHandler();
	static void RegisterScriptAction(void *ctx, calldata_t *data);
	static void DeregisterScriptAction(void *ctx, calldata_t *data);
	static void RegisterScriptCondition(void *ctx, calldata_t *data);
	static void DeregisterScriptCondition(void *ctx, calldata_t *data);
	static void GetVariableValue(void *ctx, calldata_t *data);
	static void SetVariableValue(void *ctx, calldata_t *data);

private:
	std::mutex _mutex;
	std::unordered_map<std::string, ScriptAction> _actions;
	std::unordered_map<std::string, ScriptCondition> _conditions;
};

static constexpr std::string_view GetPropertiesSignalParamName()
{
	return "properties";
}

static constexpr std::string_view GetActionCompletionSignalParamName()
{
	return "completion_signal_name";
}

static constexpr std::string_view GetConditionValueChangeSignalParamName()
{
	return "condition_value";
}

} // namespace advss
