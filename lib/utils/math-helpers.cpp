#include "math-helpers.hpp"
#include "obs-module-helper.hpp"

#include <climits>
#include <exprtk.hpp>
#include <random>

namespace advss {

std::variant<double, std::string> EvalMathExpression(const std::string &expr)
{
	static bool setupDone = false;
	static exprtk::symbol_table<double> symbolTable;
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_real_distribution<double> dis(0.0, 1.0);
	static auto randomFunc = []() {
		return dis(gen);
	};

	if (!setupDone) {
		symbolTable.add_function("random", randomFunc);
		setupDone = true;
	}

	exprtk::expression<double> expression;
	expression.register_symbol_table(symbolTable);
	exprtk::parser<double> parser;

	if (parser.compile(expr, expression)) {
		return expression.value();
	}
	return std::string(obs_module_text(
		       "AdvSceneSwitcher.math.expressionFail")) +
	       " \"" + expr + "\"";
}

bool IsValidNumber(const std::string &str)
{
	return GetDouble(str).has_value();
}

std::optional<double> GetDouble(const std::string &str)
{
	char *end = nullptr;
	double value = std::strtod(str.c_str(), &end);

	if (end != str.c_str() && *end == '\0' && value != HUGE_VAL &&
	    value != -HUGE_VAL) {
		return value;
	}

	return {};
}

std::optional<int> GetInt(const std::string &str)
{
	char *end = nullptr;
	int value = (int)std::strtol(str.c_str(), &end, 10);

	if (end != str.c_str() && *end == '\0' && value != INT_MAX &&
	    value != INT_MIN) {
		return value;
	}

	return {};
}

bool DoubleEquals(double left, double right, double epsilon)
{
	return (fabs(left - right) < epsilon);
}

} // namespace advss
