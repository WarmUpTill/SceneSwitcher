#include "math-helpers.hpp"
#include <exprtk.hpp>

#ifdef UNIT_TEST
const char *obs_module_text(const char *text)
{
	return text;
}
#else
#include <obs-module.h>
#endif

typedef exprtk::expression<double> expression_t;
typedef exprtk::parser<double> parser_t;

std::variant<double, std::string> EvalMathExpression(const std::string &expr)
{
	expression_t expression;

	parser_t parser;

	if (parser.compile(expr, expression)) {
		return expression.value();
	}
	return std::string(obs_module_text(
		       "AdvSceneSwitcher.math.expressionFail")) +
	       " \"" + expr;
}

bool IsValidNumber(const std::string &str)
{
	return GetDouble(str).has_value();
}

std::optional<double> GetDouble(const std::string &str)
{
	char *end = nullptr;
	double value = std::strtod(str.c_str(), &end);
	if (end != str.c_str() && *end == '\0' && value != HUGE_VAL) {
		return value;
	}
	return {};
}
