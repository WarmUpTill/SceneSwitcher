#include "catch.hpp"

#include <math-helpers.hpp>

TEST_CASE("Expressions are evaluated successfully", "[math-helpers]")
{
	auto expressionResult = advss::EvalMathExpression("1");
	auto *doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 1.0);

	expressionResult = advss::EvalMathExpression("-1");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == -1.0);

	expressionResult = advss::EvalMathExpression("1 + 2");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 3.0);

	expressionResult = advss::EvalMathExpression("1 + 2 * 3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 7.0);

	expressionResult = advss::EvalMathExpression("(1 + 2) * 3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 9.0);

	expressionResult = advss::EvalMathExpression("(1 - 2) * 3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == -3.0);

	expressionResult = advss::EvalMathExpression("(1-2)*3");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == -3.0);

	expressionResult =
		advss::EvalMathExpression("cos(abs(1 - sqrt(10 - 2 * 3)) - 1)");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 1.0);

	expressionResult =
		advss::EvalMathExpression("cos(abs(1 - sqrt(10 -2 * 3)) -1)");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr != nullptr);
	REQUIRE(*doubleValuePtr == 1.0);
}

TEST_CASE("Invalid expressions are not evaluated", "[math-helpers]")
{
	auto expressionResult = advss::EvalMathExpression("");
	auto *doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = advss::EvalMathExpression(")");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = advss::EvalMathExpression("(");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = advss::EvalMathExpression("()");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = advss::EvalMathExpression("1 + 2)");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);

	expressionResult = advss::EvalMathExpression("1 + 2 * asdf");
	doubleValuePtr = std::get_if<double>(&expressionResult);

	REQUIRE(doubleValuePtr == nullptr);
}

TEST_CASE("IsValidNumber", "[math-helpers]")
{
	REQUIRE(advss::IsValidNumber("1"));
	REQUIRE(advss::IsValidNumber("-1"));
	REQUIRE(advss::IsValidNumber("1.0"));
	REQUIRE(advss::IsValidNumber("-1.0"));

	REQUIRE_FALSE(advss::IsValidNumber(""));
	REQUIRE_FALSE(advss::IsValidNumber("abc"));
	REQUIRE_FALSE(advss::IsValidNumber("abc123def"));
}

TEST_CASE("GetInt", "[math-helpers]")
{
	auto result = advss::GetInt("1");
	REQUIRE(*result == 1);

	result = advss::GetInt("-1");
	REQUIRE(*result == -1);

	REQUIRE_FALSE(advss::GetInt("").has_value());
	REQUIRE_FALSE(advss::GetInt("abc123def").has_value());
	REQUIRE_FALSE(advss::GetInt("1.0").has_value());
}

TEST_CASE("GetDouble", "[math-helpers]")
{
	auto result = advss::GetDouble("1");
	REQUIRE(*result == 1.0);

	result = advss::GetDouble("1.0");
	REQUIRE(*result == 1.0);

	result = advss::GetDouble("-1.0");
	REQUIRE(*result == -1.0);

	REQUIRE_FALSE(advss::GetDouble(""));
	REQUIRE_FALSE(advss::GetDouble("abc123def"));
}

TEST_CASE("DoubleEquals", "[math-helpers]")
{
	REQUIRE(advss::DoubleEquals(1.0, 1.0, 0.1));
	REQUIRE(advss::DoubleEquals(10.0, 11.0, 2.0));
	REQUIRE_FALSE(advss::DoubleEquals(10.0, 1.0, 0.1));
	REQUIRE_FALSE(advss::DoubleEquals(1.0, 2.0, 0.5));
	REQUIRE_FALSE(advss::DoubleEquals(1.0, 1.0, 0.0));
}
