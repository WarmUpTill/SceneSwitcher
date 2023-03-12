#include "math-helpers.hpp"

#include <string_view>
#include <algorithm>
#include <vector>
#include <stack>
#include <cmath>

#ifdef UNIT_TEST
const char *obs_module_text(const char *text)
{
	return text;
}
#else
#include <obs-module.h>
#endif

static std::vector<std::string_view> nonOperatorSeparators = {
	" ",
	"\t",
};

static std::vector<std::string_view> brackets = {
	"(",
	")",
};

static std::vector<std::string_view> unaryOperators = {
	"abs(", "sin(", "cos(", "tan(", "sqrt(",
};

static std::vector<std::string_view> binaryOperators = {
	"+",
	"-",
	"*",
	"/",
};

static bool isBracket(const std::string &token)
{
	return std::find(brackets.begin(), brackets.end(), token) !=
	       brackets.end();
}

static bool isUnaryOperator(const std::string &token)
{
	return std::find(unaryOperators.begin(), unaryOperators.end(), token) !=
	       unaryOperators.end();
}

static bool isBinaryOperator(const std::string &token)
{
	return std::find(binaryOperators.begin(), binaryOperators.end(),
			 token) != binaryOperators.end();
}

static bool isOperator(const std::string &token)
{
	return isUnaryOperator(token) || isBinaryOperator(token);
}

static void addOperatorToken(std::vector<std::string> &tokens,
			     const std::string &separator)
{
	if (separator.back() != '(') {
		tokens.push_back(separator);
		return;
	}
	tokens.push_back("(");
	tokens.push_back(separator);
}

static std::vector<std::string> splitStringIntoTokens(const std::string &input)
{
	auto separators = nonOperatorSeparators;
	separators.insert(separators.end(), brackets.begin(), brackets.end());
	separators.insert(separators.end(), unaryOperators.begin(),
			  unaryOperators.end());
	separators.insert(separators.end(), binaryOperators.begin(),
			  binaryOperators.end());

	std::vector<std::string> tokens;
	std::string::size_type start = 0;

	while (start < input.length()) {
		std::string::size_type min_pos = std::string::npos;
		std::string separator;

		// Find the next occurrence of each separator
		for (const auto &sep : separators) {
			std::string::size_type pos = input.find(sep, start);
			if (pos != std::string::npos && pos < min_pos) {
				min_pos = pos;
				separator = sep;
			}
		}

		// If a separator was found, add the token before it to the vector
		if (min_pos != std::string::npos) {
			const auto token = input.substr(start, min_pos - start);
			start = min_pos + separator.length();

			if (!std::all_of(token.begin(), token.end(), isspace)) {
				tokens.push_back(token);
			}

			// If the separator itself was an operator, add it as a
			// token to the vector
			if (isOperator(separator)) {
				addOperatorToken(tokens, separator);
			}
			if (isBracket(separator)) {
				tokens.push_back(separator);
			}
		}
		// Otherwise, add the remaining string to the vector and exit the
		// loop
		else {
			tokens.push_back(input.substr(start));
			break;
		}
	}

	return tokens;
}

static int precedence(const std::string &op)
{
	if (op == "*" || op == "/") {
		return 2;
	} else if (op == "+" || op == "-") {
		return 1;
	} else {
		return 0;
	}
}

static std::string getErrorMsg(const std::string &expr, const std::string &msg)
{
	return std::string(obs_module_text(
		       "AdvSceneSwitcher.math.expressionFail")) +
	       " \"" + expr + "\":\n" + msg;
}

static std::string getErrorMsg(const std::string &expr,
			       std::stack<std::string> binaryOperators,
			       std::stack<double> operands,
			       const std::string &msg)
{
	std::string errMsg = getErrorMsg(expr, msg);
	errMsg += "\n\n ---" +
		  std::string(
			  obs_module_text("AdvSceneSwitcher.math.operators")) +
		  " ---\n";
	while (!binaryOperators.empty()) {
		errMsg += binaryOperators.top() + "\n";
		binaryOperators.pop();
	}

	errMsg +=
		"\n\n --- " +
		std::string(obs_module_text("AdvSceneSwitcher.math.operands")) +
		" ---\n";
	while (!operands.empty()) {
		errMsg += std::to_string(operands.top()) + "\n";
		operands.pop();
	}

	return errMsg;
}

static bool evalHelper(std::stack<std::string> &operators,
		       std::stack<double> &operands)
{
	std::string op = operators.top();
	operators.pop();

	if (isUnaryOperator(op)) {
		if (operands.size() < 1) {
			return false;
		}

		double value = operands.top();
		operands.pop();
		double result;
		if (op == "abs(") {
			result = abs(value);
		} else if (op == "sin(") {
			result = sin(value);
		} else if (op == "cos(") {
			result = cos(value);
		} else if (op == "tan(") {
			result = cos(value);
		} else if (op == "sqrt(") {
			result = sqrt(value);
		}
		operands.push(result);
		return true;
	}

	if (operands.size() < 2) {
		return false;
	}
	double op2 = operands.top();
	operands.pop();
	double op1 = operands.top();
	operands.pop();
	double result;

	if (op == "+") {
		result = op1 + op2;
	} else if (op == "-") {
		result = op1 - op2;
	} else if (op == "*") {
		result = op1 * op2;
	} else if (op == "/") {
		result = op1 / op2;
	}
	operands.push(result);
	return true;
}

std::variant<double, std::string>
EvalMathExpression(const std::string &expression)
{
	// Create a stack to store operands and operators
	std::stack<double> operands;
	std::stack<std::string> operators;

	auto tokens = splitStringIntoTokens(expression);

	// Loop through each token in the expression
	for (const auto &token : tokens) {
		// If the token is a number, push it onto the operand stack
		if (isdigit(token[0])) {
			auto operand = GetDouble(token);
			if (!operand.has_value()) {
				return getErrorMsg(
					expression,
					"\"" + token + "\" " +
						obs_module_text(
							"AdvSceneSwitcher.math.notANumber"));
			}
			operands.push(*operand);
		}
		// If the token is an operator, evaluate higher-precedence
		// operators first
		else if (isBinaryOperator(token)) {
			while (!operators.empty() &&
			       precedence(operators.top()) >=
				       precedence(token)) {
				if (operators.empty() || operands.empty()) {
					return getErrorMsg(expression,
							   operators, operands,
							   "");
				}
				if (!evalHelper(operators, operands)) {
					return getErrorMsg(expression,
							   operators, operands,
							   "");
				}
			}
			operators.push(token);
		}
		// If the token is an opening bracket, push it onto the
		// operator stack
		else if (token == "(") {
			operators.push(token);
		}
		// If the token is a closing bracket, evaluate the expression
		// inside the brackets
		else if (token == ")") {
			if (operators.empty()) {
				return getErrorMsg(
					expression, operators, operands,
					obs_module_text(
						"AdvSceneSwitcher.math.expressionFailParentheses"));
			}
			while (operators.top() != "(") {
				if (operators.empty() || operands.empty()) {
					return getErrorMsg(expression,
							   operators, operands,
							   "");
				}
				if (!evalHelper(operators, operands) ||
				    operators.empty()) {
					return getErrorMsg(
						expression, operators, operands,
						obs_module_text(
							"AdvSceneSwitcher.math.expressionFailParentheses"));
				}
			}
			operators.pop(); // Pop the opening bracket

		}

		else if (isUnaryOperator(token)) {
			operators.push(token);
		}

		else if (!token.empty()) {
			return getErrorMsg(
				expression,
				std::string(obs_module_text(
					"AdvSceneSwitcher.math.invalidToken")) +
					" \"" + token + "\"");
		}
	}

	// Evaluate any remaining operators in the stack
	while (!operators.empty()) {
		if (!evalHelper(operators, operands) || operands.empty()) {
			return getErrorMsg(expression, operators, operands, "");
		}
	}

	// The result is the top operand on the stack
	if (operands.size() != 1) {
		return getErrorMsg(
			expression, operators, operands,
			std::string(obs_module_text(
				"AdvSceneSwitcher.math.notFullyResolved")));
	}
	return operands.top();
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
