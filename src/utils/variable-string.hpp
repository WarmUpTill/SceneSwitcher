#pragma once
#include "variable.hpp"

#include <string>
#include <obs.hpp>

namespace advss {

// Helper class which automatically resovles variables contained in strings
// when reading its value as a std::string

class StringVariable {
public:
	StringVariable() : _value(""){};
	StringVariable(std::string str) : _value(std::move(str)){};
	StringVariable(const char *str) : _value(str){};
	operator std::string();
	operator QVariant() const;
	void operator=(std::string);
	void operator=(const char *value);
	const char *c_str();
	const char *c_str() const;

	const std::string &UnresolvedValue() const { return _value; }

	void Load(obs_data_t *obj, const char *name);
	void Save(obs_data_t *obj, const char *name) const;

private:
	void Resolve();

	std::string _value = "";
	std::string _resolvedValue = "";
	std::chrono::high_resolution_clock::time_point _lastResolve{};
};

std::string SubstitueVariables(std::string str);

} // namespace advss

Q_DECLARE_METATYPE(advss::StringVariable);