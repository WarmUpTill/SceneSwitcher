#pragma once
#include "variable.hpp"

#include <string>
#include <obs-data.h>

namespace advss {

// Helper class which automatically resolves variables contained in strings
// when reading its value as a std::string

class StringVariable {
public:
	EXPORT StringVariable() : _value(""){};
	EXPORT StringVariable(std::string str) : _value(std::move(str)){};
	EXPORT StringVariable(const char *str) : _value(str){};
	EXPORT operator std::string() const;
	EXPORT operator QVariant() const;
	EXPORT void operator=(std::string);
	EXPORT void operator=(const char *value);
	EXPORT const char *c_str();
	EXPORT const char *c_str() const;
	EXPORT bool empty() const;

	EXPORT const std::string &UnresolvedValue() const { return _value; }

	EXPORT void Load(obs_data_t *obj, const char *name);
	EXPORT void Save(obs_data_t *obj, const char *name) const;

private:
	void Resolve() const;

	std::string _value = "";
	mutable std::string _resolvedValue = "";
	mutable std::chrono::high_resolution_clock::time_point _lastResolve{};
};

std::string SubstitueVariables(std::string str);

} // namespace advss

Q_DECLARE_METATYPE(advss::StringVariable);
