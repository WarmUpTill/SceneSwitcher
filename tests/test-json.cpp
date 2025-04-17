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

TEST_CASE("GetJsonField", "[json-helpers]")
{
	auto result = advss::GetJsonField("{}", "");
	REQUIRE_FALSE(result);

	result = advss::GetJsonField("{}", "not there");
	REQUIRE_FALSE(result);

	result = advss::GetJsonField("invalid json", "not there");
	REQUIRE_FALSE(result);

	result = advss::GetJsonField("{ \"test\": 1 }", "test");
	REQUIRE(*result == "1");
}

TEST_CASE("QueryJson", "[json-helpers]")
{
	auto result = advss::QueryJson("{}", "");
	REQUIRE_FALSE(result);

	result = advss::QueryJson("invalid json", "$.test");
	REQUIRE_FALSE(result);

	result = advss::QueryJson("{}", "invalid query");
	REQUIRE_FALSE(result);

	result = advss::QueryJson("{}", "$.test");
	REQUIRE(*result == "[]");

	result = advss::QueryJson("{ \"test\": { \"something\" : 1 } }",
				  "$.test.something");
	REQUIRE(*result == "[1]");
}

TEST_CASE("AccessJsonArrayIndex", "[json-helpers]")
{
	auto result = advss::AccessJsonArrayIndex("{}", 0);
	REQUIRE_FALSE(result);

	result = advss::AccessJsonArrayIndex("invalid json", 0);
	REQUIRE_FALSE(result);

	result = advss::AccessJsonArrayIndex("[]", 0);
	REQUIRE_FALSE(result);

	result = advss::AccessJsonArrayIndex("[\"1\", \"2\"]", -1);
	REQUIRE_FALSE(result);

	result = advss::AccessJsonArrayIndex("[\"1\", \"2\"]", 1);
	REQUIRE(*result == "2");
}
