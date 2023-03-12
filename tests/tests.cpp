#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <math-helpers.hpp>

TEST_CASE("Expressions are evaluated successfully", "[math-helpers]")
{
	auto expressionResult = EvalMathExpression("1");
	auto *doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 1.0);

	expressionResult = EvalMathExpression("1 + 2");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 3.0);

	expressionResult = EvalMathExpression("1 + 2 * 3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 7.0);

	expressionResult = EvalMathExpression("(1 + 2) * 3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 9.0);

	expressionResult =
		EvalMathExpression("cos(abs(1 - sqrt(10 - 2 * 3)) -1)");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 1.0);
}

TEST_CASE("Invalid expressions are not evaluated", "[math-helpers]")
{
	auto expressionResult = EvalMathExpression("");
	auto *doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = EvalMathExpression(")");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = EvalMathExpression("(");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = EvalMathExpression("()");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = EvalMathExpression("1 + 2)");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = EvalMathExpression("1 + 2 * asdf");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);
}
