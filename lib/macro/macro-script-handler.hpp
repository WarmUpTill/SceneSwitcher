#pragma once
#include <obs.hpp>
#include <mutex>
#include <string>
#include <unordered_map>

namespace advss {

class ScriptAction {
public:
	ScriptAction() = delete;
	ScriptAction(const std::string &id, bool blocking,
		     const std::string &signal,
		     const std::string &signalComplete);

private:
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

static constexpr std::string_view GetActionCompletionSignalParamName()
{
	return "completion_signal_name";
}

static constexpr std::string_view GetConditionValueChangeSignalParamName()
{
	return "condition_value";
}

} // namespace advss
