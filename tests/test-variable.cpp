#include "catch.hpp"

#include <variable.hpp>
#include <thread>

TEST_CASE("Variable", "[variable]")
{
	advss::Variable variable;
	REQUIRE_FALSE(variable.GetSecondsSinceLastUse());

	REQUIRE(variable.Value() == "");
	REQUIRE(variable.GetPreviousValue() == "");
	REQUIRE(variable.GetDefaultValue() == "");
	REQUIRE(variable.GetSaveAction() ==
		advss::Variable::SaveAction::DONT_SAVE);
	REQUIRE(variable.GetValueChangeCount() == 0);

	variable.SetValue("testing");
	REQUIRE(variable.Value() == "testing");
	REQUIRE(variable.GetPreviousValue() == "");
	REQUIRE(variable.GetValueChangeCount() == 1);

	variable.SetValue(123);
	REQUIRE(variable.Value() == "123");
	REQUIRE(variable.GetPreviousValue() == "testing");

	variable.SetValue(123.0);
	REQUIRE(variable.Value() == "123");

	variable.SetValue(123.123);
	REQUIRE(variable.Value() == "123.123");

	REQUIRE(*variable.GetSecondsSinceLastUse() == 0);
	REQUIRE(*variable.GetSecondsSinceLastChange() == 0);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	REQUIRE(*variable.GetSecondsSinceLastUse() > 1);
	REQUIRE(*variable.GetSecondsSinceLastChange() > 1);

	variable.Value(false);
	REQUIRE(*variable.GetSecondsSinceLastUse() > 1);

	variable.UpdateLastUsed();
	REQUIRE(*variable.GetSecondsSinceLastUse() == 0);

	variable.SetValue(123);
	REQUIRE(*variable.GetSecondsSinceLastChange() == 0);

	std::this_thread::sleep_for(std::chrono::seconds(1));
	variable.SetValue(123);
	REQUIRE(*variable.GetSecondsSinceLastChange() > 0);
}
