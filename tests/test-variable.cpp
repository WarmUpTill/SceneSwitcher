#include "catch.hpp"

#include <variable.hpp>
#include <thread>

TEST_CASE("Variable", "[variable]")
{
	advss::Variable variable;
	REQUIRE_FALSE(variable.SecondsSinceLastUse());

	REQUIRE(variable.Value() == "");
	REQUIRE(variable.GetDefaultValue() == "");
	REQUIRE(variable.GetSaveAction() ==
		advss::Variable::SaveAction::DONT_SAVE);

	variable.SetValue("testing");
	REQUIRE(variable.Value() == "testing");

	variable.SetValue(123);
	REQUIRE(variable.Value() == "123");

	variable.SetValue(123.0);
	REQUIRE(variable.Value() == "123");

	variable.SetValue(123.123);
	REQUIRE(variable.Value() == "123.123");

	REQUIRE(*variable.SecondsSinceLastUse() == 0);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	REQUIRE(*variable.SecondsSinceLastUse() > 1);

	variable.Value(false);
	REQUIRE(*variable.SecondsSinceLastUse() > 1);

	variable.UpdateLastUsed();
	REQUIRE(*variable.SecondsSinceLastUse() == 0);
}
