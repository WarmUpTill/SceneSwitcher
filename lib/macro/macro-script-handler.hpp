#pragma once
#include <obs.hpp>
#include <mutex>
#include <string>
#include <unordered_map>

namespace advss {

class ScriptSegmentType {
public:
	ScriptSegmentType() = delete;
	ScriptSegmentType(const std::string &id,
			  const std::string &propertiesSignal,
			  const std::string &triggerSignal,
			  const std::string &completionSignal);

private:
	std::string _id;
};

class ScriptHandler {
public:
	ScriptHandler() = delete;
	static void RegisterScriptAction(void *ctx, calldata_t *data);
	static void DeregisterScriptAction(void *ctx, calldata_t *data);
	static void RegisterScriptCondition(void *ctx, calldata_t *data);
	static void DeregisterScriptCondition(void *ctx, calldata_t *data);
	static void GetVariableValue(void *ctx, calldata_t *data);
	static void SetVariableValue(void *ctx, calldata_t *data);
	static bool ActionIdIsValid(const std::string &id);
	static bool ConditionIdIsValid(const std::string &id);

private:
	static std::mutex _mutex;
	static std::unordered_map<std::string, ScriptSegmentType> _actions;
	static std::unordered_map<std::string, ScriptSegmentType> _conditions;
};

static constexpr std::string_view GetPropertiesSignalParamName()
{
	return "properties";
}

static constexpr std::string_view GetActionCompletionSignalParamName()
{
	return "completion_signal_name";
}

static constexpr std::string_view GetCompletionIdParamName()
{
	return "completion_id";
}

static constexpr std::string_view GetResultSignalParamName()
{
	return "result";
}

} // namespace advss
