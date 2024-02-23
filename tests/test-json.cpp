#include "catch.hpp"

#include <json-helpers.hpp>

TEST_CASE("FormatJsonString", "[json-helpers]")
{
	auto result = advss::FormatJsonString(QString(""));
	REQUIRE(result == "");

	result = advss::FormatJsonString(QString("abc"));
	REQUIRE(result == "");

	result = advss::FormatJsonString(QString("{}"));
	REQUIRE(result == "{\n}\n");

	result = advss::FormatJsonString(QString("{\"test\":true}"));
	REQUIRE(result == "{\n    \"test\": true\n}\n");
}

TEST_CASE("MatchJson", "[json-helpers]")
{
	advss::RegexConfig regex;
	bool result = advss::MatchJson("", "", regex);
	REQUIRE(result == true);

	result = advss::MatchJson("a", "", regex);
	REQUIRE(result == false);

	result = advss::MatchJson("{}", "abc", regex);
	REQUIRE(result == false);

	result = advss::MatchJson("{}", "{}", regex);
	REQUIRE(result == true);

	result = advss::MatchJson("{\"test\":true}",
				  "{\n    \"test\": true\n}\n", regex);
	REQUIRE(result == true);

	regex.SetEnabled(true);
	result = advss::MatchJson("", "{\n    \"test\": true\n}\n", regex);
	REQUIRE(result == false);

	result = advss::MatchJson("{\"test\":true}",
				  "{\n    \"test\": true\n}\n", regex);
	REQUIRE(result == true);

	result = advss::MatchJson("{\n    \"test\": true\n}\n", "(.|\\n)*",
				  regex);
	REQUIRE(result == true);

	result = advss::MatchJson("{\n    \"test\": true\n}\n", "(", regex);
	REQUIRE(result == false);
}
