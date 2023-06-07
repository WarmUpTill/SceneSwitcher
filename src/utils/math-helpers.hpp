#pragma once
#include <string>
#include <variant>
#include <optional>

namespace advss {

std::variant<double, std::string>
EvalMathExpression(const std::string &expression);
bool IsValidNumber(const std::string &);
std::optional<double> GetDouble(const std::string &);
std::optional<int> GetInt(const std::string &);

} // namespace advss
