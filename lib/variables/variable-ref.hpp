#pragma once
#include "export-symbol-helper.hpp"

#include <memory>
#include <obs-data.h>
#include <string>
#include <unordered_map>

namespace advss {

class Variable;

// While an instance is alive, variable references saved on the current
// thread are counted by variable name.
class VariableRefCollector {
public:
	EXPORT VariableRefCollector();
	EXPORT ~VariableRefCollector();
	VariableRefCollector(const VariableRefCollector &) = delete;
	VariableRefCollector &operator=(const VariableRefCollector &) = delete;

	EXPORT void Add(const std::string &name, int count = 1);
	const std::unordered_map<std::string, int> &Counts() const
	{
		return _counts;
	}

private:
	std::unordered_map<std::string, int> _counts;
	VariableRefCollector *_previous = nullptr;
};

EXPORT void ReportVariableRef(const std::weak_ptr<Variable> &variable);
EXPORT void ReportVariableRefsInText(const std::string &text);

class VariableRef {
public:
	VariableRef() = default;
	VariableRef(const std::weak_ptr<Variable> &variable)
		: _variable(variable)
	{
	}
	VariableRef &operator=(const std::weak_ptr<Variable> &variable)
	{
		_variable = variable;
		return *this;
	}
	operator std::weak_ptr<Variable>() const { return _variable; }
	std::shared_ptr<Variable> lock() const { return _variable.lock(); }
	bool expired() const { return _variable.expired(); }

	EXPORT void Save(obs_data_t *obj, const char *key) const;
	EXPORT void Load(obs_data_t *obj, const char *key);

private:
	std::weak_ptr<Variable> _variable;
};

} // namespace advss
