#pragma once
#include "export-symbol-helper.hpp"

#include <string>
#include <variant>
#include <optional>

namespace advss {

std::variant<double, std::string>
EvalMathExpression(const std::string &expression);
bool IsValidNumber(const std::string &);
EXPORT std::optional<double> GetDouble(const std::string &);
EXPORT std::optional<int> GetInt(const std::string &);

} // namespace advss
