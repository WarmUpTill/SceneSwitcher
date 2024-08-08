#include "catch.hpp"

#include <condition-logic.hpp>

TEST_CASE("Get and set", "[conditon-logic]")
{
	auto logic = advss::Logic(advss::Logic::Type::ROOT_NONE);
	REQUIRE(logic.GetType() == advss::Logic::Type::ROOT_NONE);

	logic.SetType(advss::Logic::Type::ROOT_NOT);
	REQUIRE(logic.GetType() == advss::Logic::Type::ROOT_NOT);

	logic.SetType(advss::Logic::Type::AND_NOT);
	REQUIRE(logic.GetType() == advss::Logic::Type::AND_NOT);
}

TEST_CASE("Root test", "[conditon-logic]")
{
	auto logic = advss::Logic(advss::Logic::Type::ROOT_NONE);
	REQUIRE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::ROOT_NOT);
	REQUIRE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::ROOT_LAST);
	REQUIRE(logic.IsRootType());

	logic.SetType(advss::Logic::Type::NONE);
	REQUIRE_FALSE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::AND);
	REQUIRE_FALSE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::OR);
	REQUIRE_FALSE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::AND_NOT);
	REQUIRE_FALSE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::OR_NOT);
	REQUIRE_FALSE(logic.IsRootType());
	logic.SetType(advss::Logic::Type::LAST);
	REQUIRE_FALSE(logic.IsRootType());
}

TEST_CASE("Negation test", "[conditon-logic]")
{
	REQUIRE_FALSE(
		advss::Logic::IsNegationType(advss::Logic::Type::ROOT_NONE));
	REQUIRE(advss::Logic::IsNegationType(advss::Logic::Type::ROOT_NOT));
	REQUIRE_FALSE(
		advss::Logic::IsNegationType(advss::Logic::Type::ROOT_LAST));
	REQUIRE_FALSE(advss::Logic::IsNegationType(advss::Logic::Type::NONE));
	REQUIRE_FALSE(advss::Logic::IsNegationType(advss::Logic::Type::AND));
	REQUIRE_FALSE(advss::Logic::IsNegationType(advss::Logic::Type::OR));
	REQUIRE(advss::Logic::IsNegationType(advss::Logic::Type::AND_NOT));
	REQUIRE(advss::Logic::IsNegationType(advss::Logic::Type::OR_NOT));
	REQUIRE_FALSE(advss::Logic::IsNegationType(advss::Logic::Type::LAST));
}

TEST_CASE("Validation", "[conditon-logic]")
{
	auto logic = advss::Logic(static_cast<advss::Logic::Type>(-1));
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));

	logic = advss::Logic(static_cast<advss::Logic::Type>(
		static_cast<int>(advss::Logic::Type::LAST) + 1));
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));

	logic.SetType(advss::Logic::Type::ROOT_NONE);
	REQUIRE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::ROOT_NOT);
	REQUIRE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));

	logic.SetType(advss::Logic::Type::NONE);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::AND);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::OR);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::AND_NOT);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::OR_NOT);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE(logic.IsValidSelection(false));

	logic.SetType(advss::Logic::Type::ROOT_LAST);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));
	logic.SetType(advss::Logic::Type::LAST);
	REQUIRE_FALSE(logic.IsValidSelection(true));
	REQUIRE_FALSE(logic.IsValidSelection(false));
}

TEST_CASE("Logic", "[conditon-logic]")
{
	// These logic types should have no effect on the initial provided value

	auto logic =
		advss::Logic(static_cast<advss::Logic::Type>(-1)).GetType();
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::NONE;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::LAST;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	// These logic types should have an effect on the initial provided value

	logic = advss::Logic::Type::ROOT_NONE;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::ROOT_NOT;
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE_FALSE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::AND;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::OR;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::AND_NOT;
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE_FALSE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));

	logic = advss::Logic::Type::OR_NOT;
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, false, false, ""));
	REQUIRE_FALSE(
		advss::Logic::ApplyConditionLogic(logic, false, true, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, false, ""));
	REQUIRE(advss::Logic::ApplyConditionLogic(logic, true, true, ""));
}
