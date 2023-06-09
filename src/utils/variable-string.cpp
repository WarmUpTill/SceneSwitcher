#include "variable-string.hpp"
#include "switcher-data.hpp"
#include "utility.hpp"

namespace advss {

void StringVariable::Resolve() const
{
	if (switcher && switcher->variables.empty()) {
		_resolvedValue = _value;
		return;
	}
	if (_lastResolve == GetLastVariableChangeTime()) {
		return;
	}
	_resolvedValue = SubstitueVariables(_value);
	_lastResolve = GetLastVariableChangeTime();
}

StringVariable::operator std::string() const
{
	Resolve();
	return _resolvedValue;
}

StringVariable::operator QVariant() const
{
	return QVariant::fromValue<StringVariable>(*this);
}

void StringVariable::operator=(std::string value)
{
	_value = value;
	_lastResolve = {};
}

void StringVariable::operator=(const char *value)
{
	_value = value;
	_lastResolve = {};
}

void StringVariable::Load(obs_data_t *obj, const char *name)
{
	_value = obs_data_get_string(obj, name);
	Resolve();
}

void StringVariable::Save(obs_data_t *obj, const char *name) const
{
	obs_data_set_string(obj, name, _value.c_str());
}

const char *StringVariable::c_str()
{
	Resolve();
	return _resolvedValue.c_str();
}

const char *StringVariable::c_str() const
{
	Resolve();
	return _resolvedValue.c_str();
}

std::string SubstitueVariables(std::string str)
{
	if (!switcher) {
		return str;
	}

	for (const auto &v : switcher->variables) {
		const auto &variable = std::dynamic_pointer_cast<Variable>(v);
		const std::string pattern = "${" + variable->Name() + "}";
		ReplaceAll(str, pattern, variable->Value());
	}
	return str;
}

} // namespace advss
