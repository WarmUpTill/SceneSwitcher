#include "catch.hpp"

#include <json-helpers.hpp>
#include <text-helpers.hpp>

TEST_CASE("Enable", "[regex-config]")
{
	advss::RegexConfig regex(false);
	REQUIRE(regex.Enabled() == false);

	regex.SetEnabled(true);
	REQUIRE(regex.Enabled() == true);
}

TEST_CASE("Matches (default options)", "[regex-config]")
{
	advss::RegexConfig regex(true);

	bool result = regex.Matches(std::string(""), "");
	REQUIRE(result == true);

	result = regex.Matches(std::string("a"), "a");
	REQUIRE(result == true);

	result = regex.Matches(std::string("a"), "");
	REQUIRE(result == false);

	result = regex.Matches(std::string("a"), "abc");
	REQUIRE(result == false);

	result = regex.Matches(std::string("abc"), "abc");
	REQUIRE(result == true);

	result = regex.Matches(std::string("abc"), "a");
	REQUIRE(result == false);
}

TEST_CASE("Matches (regex options)", "[regex-config]")
{
	advss::RegexConfig regex(true);

	bool result = regex.Matches(std::string("abc"), "abc");
	REQUIRE(result == true);

	result = regex.Matches(std::string("abc"), "Abc");
	REQUIRE(result == false);

	regex.SetPatternOptions(QRegularExpression::CaseInsensitiveOption);
	result = regex.Matches(std::string("abc"), "Abc");
	REQUIRE(result == true);
}

TEST_CASE("Matches (invalid expression)", "[regex-config]")
{
	advss::RegexConfig regex(true);
	bool result = regex.Matches(std::string("a"), "(");
	REQUIRE(result == false);

	regex = advss::RegexConfig::PartialMatchRegexConfig();
	result = regex.Matches(std::string("abc"), "(");
	REQUIRE(result == false);
}

TEST_CASE("Matches (PartialMatchRegexConfig)", "[regex-config]")
{
	auto regex = advss::RegexConfig::PartialMatchRegexConfig();
	regex.SetEnabled(true);
	bool result = regex.Matches(std::string(""), "");
	REQUIRE(result == true);

	result = regex.Matches(std::string("a"), "");
	REQUIRE(result == true);

	result = regex.Matches(std::string("a"), "abc");
	REQUIRE(result == false);

	result = regex.Matches(std::string("abc"), "abc");
	REQUIRE(result == true);

	result = regex.Matches(std::string("abc"), "a");
	REQUIRE(result == true);
}

TEST_CASE("EscapeForRegex , [text-helpers]")
{
	REQUIRE(advss::EscapeForRegex("") == "");
	REQUIRE(advss::EscapeForRegex("abcdefg") == "abcdefg");
	REQUIRE(advss::EscapeForRegex("(abcdefg)") == "\\(abcdefg\\)");
	REQUIRE(advss::EscapeForRegex("\\(abcdefg)") == "\\\\(abcdefg\\)");
}
