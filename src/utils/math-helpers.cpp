#include "math-helpers.hpp"

#include <string_view>
#include <algorithm>
#include <vector>
#include <stack>
#include <cmath>

#include <obs-module.h>

static std::vector<std::string_view> nonOperatorSeparators = {
	" ",
	"\t",
};

static std::vector<std::string_view> operators = {
	"+", "-", "*", "/", "(", ")",
};

static std::vector<std::string> splitStringIntoTokens(const std::string &input)
{
	auto separators = nonOperatorSeparators;
	separators.insert(separators.end(), operators.begin(), operators.end());
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
			tokens.push_back(input.substr(start, min_pos - start));
			start = min_pos + separator.length();

			// If the separator itself was an operator, add it as a
			// token to the vector
			if (std::find(operators.begin(), operators.end(),
				      separator) != operators.end()) {
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
			       std::stack<std::string> operators,
			       std::stack<double> operands,
			       const std::string &msg)
{
	std::string errMsg = getErrorMsg(expr, msg);
	errMsg += "\n\n ---" +
		  std::string(
			  obs_module_text("AdvSceneSwitcher.math.operators")) +
		  " ---\n";
	while (!operators.empty()) {
		errMsg += operators.top() + "\n";
		operators.pop();
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

static void evalHelper(std::stack<std::string> &operators,
		       std::stack<double> &operands)
{
	std::string op = operators.top();
	operators.pop();
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
		else if (token == "+" || token == "-" || token == "*" ||
			 token == "/") {
			while (!operators.empty() &&
			       precedence(operators.top()) >=
				       precedence(token)) {
				if (operators.empty() || operands.size() < 2) {
					return getErrorMsg(expression,
							   operators, operands,
							   "");
				}
				evalHelper(operators, operands);
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
			while (operators.top() != "(") {
				if (operators.empty() || operands.size() < 2) {
					return getErrorMsg(expression,
							   operators, operands,
							   "");
				}
				evalHelper(operators, operands);
				if (operators.empty()) {
					return getErrorMsg(
						expression, operators, operands,
						obs_module_text(
							"AdvSceneSwitcher.math.expressionFailParentheses"));
				}
			}
			operators.pop(); // Pop the opening bracket

		}

		// TODO: Add function "operators"
		//
		// If the token is a function, pop the top operand and apply
		// the function
		// else if (token == "sin" || token == "cos" || token == "tan" ||
		// 	 token == "sqrt") {
		// 	double op = operands.top();
		// 	operands.pop();
		// 	double result;
		// 	if (token == "sin") {
		// 		result = sin(op);
		// 	} else if (token == "cos") {
		// 		result = cos(op);
		// 	} else if (token == "tan") {
		// 		result = tan(op);
		// 	} else if (token == "sqrt") {
		// 		result = sqrt(op);
		// 	}
		// 	operands.push(result);
		// }

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
		if (operators.empty() || operands.size() < 2) {
			return getErrorMsg(expression, operators, operands, "");
		}
		evalHelper(operators, operands);
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
