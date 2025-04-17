#include "catch.hpp"

#include <utility.hpp>

TEST_CASE("ReplaceAll", "[utility]")
{
	std::string input;
	auto result = advss::ReplaceAll(input, "", "");
	REQUIRE_FALSE(result);
	REQUIRE(input == "");

	input = "testing";
	result = advss::ReplaceAll(input, "not there", "sleep");
	REQUIRE_FALSE(result);
	REQUIRE(input == "testing");

	input = "testing";
	result = advss::ReplaceAll(input, "test", "sleep");
	REQUIRE(result);
	REQUIRE(input == "sleeping");
}

TEST_CASE("CompareIgnoringLineEnding", "[utility]")
{
	QString s1;
	QString s2;
	REQUIRE(advss::CompareIgnoringLineEnding(s1, s2));

	s1 = "test";
	s2 = "test";
	REQUIRE(advss::CompareIgnoringLineEnding(s1, s2));

	s1 = "test";
	s2 = "something else";
	REQUIRE_FALSE(advss::CompareIgnoringLineEnding(s1, s2));

	s1 = "test\r\nwith line ending";
	s2 = "test\nwith line ending";
	REQUIRE(advss::CompareIgnoringLineEnding(s1, s2));
}

TEST_CASE("ToString", "[utility]")
{
	REQUIRE(advss::ToString(1.0) == "1");
	REQUIRE(advss::ToString(-1.0) == "-1");
	REQUIRE(advss::ToString(-1.0) == "-1");
	REQUIRE(advss::ToString(-1.23) == "-1.23");
}
