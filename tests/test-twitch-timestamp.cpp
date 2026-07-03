#include "catch.hpp"

#include <twitch-timestamp.hpp>

#include <chrono>
#include <cstdio>
#include <ctime>
#include <string>

// Build an RFC3339 UTC timestamp offset by 'offsetSeconds' from now.
// A positive value means that many seconds in the past.
static std::string makeTimestamp(int offsetSeconds = 0, bool withNanos = false,
				 const char *tzSuffix = "Z")
{
	auto now = std::chrono::system_clock::now() -
		   std::chrono::seconds(offsetSeconds);
	time_t t = std::chrono::system_clock::to_time_t(now);
	std::tm tm = {};
#ifdef _WIN32
	gmtime_s(&tm, &t);
#else
	gmtime_r(&t, &tm);
#endif
	char buf[64];
	if (withNanos) {
		snprintf(buf, sizeof(buf),
			 "%04d-%02d-%02dT%02d:%02d:%02d.123456789%s",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec, tzSuffix);
	} else {
		snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d%s",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec, tzSuffix);
	}
	return buf;
}

TEST_CASE("Valid timestamps are accepted", "[twitch][timestamp]")
{
	SECTION("Current time with Z suffix")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(makeTimestamp(0)));
	}

	SECTION("5 seconds in the past")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(makeTimestamp(5)));
	}

	SECTION("9 minutes in the past (within 10-minute window)")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(makeTimestamp(9 * 60)));
	}

	SECTION("Timestamp with nanoseconds and Z suffix")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(
			makeTimestamp(0, true, "Z")));
	}

	SECTION("Lowercase z suffix is rejected (RFC 3339 requires uppercase Z)")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			makeTimestamp(0, false, "z")));
	}

	SECTION("+00:00 offset (UTC expressed as positive offset)")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(
			makeTimestamp(0, false, "+00:00")));
	}

	SECTION("-00:00 offset")
	{
		REQUIRE(advss::IsValidEventSubTimestamp(
			makeTimestamp(0, false, "-00:00")));
	}

	SECTION("+05:30 offset (IST) with nanoseconds")
	{
		// Build a timestamp whose wall-clock value is now, expressed in
		// IST (+05:30). The UTC equivalent must still be within the window.
		auto now = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(now);
		// Advance the displayed time by 5h30m to represent +05:30
		t += (5 * 60 + 30) * 60;
		std::tm tm = {};
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		char buf[64];
		snprintf(buf, sizeof(buf),
			 "%04d-%02d-%02dT%02d:%02d:%02d.000000000+05:30",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec);
		REQUIRE(advss::IsValidEventSubTimestamp(buf));
	}

	SECTION("-08:00 offset (PST)")
	{
		auto now = std::chrono::system_clock::now();
		time_t t = std::chrono::system_clock::to_time_t(now);
		t -= 8 * 3600;
		std::tm tm = {};
#ifdef _WIN32
		gmtime_s(&tm, &t);
#else
		gmtime_r(&t, &tm);
#endif
		char buf[64];
		snprintf(buf, sizeof(buf),
			 "%04d-%02d-%02dT%02d:%02d:%02d-08:00",
			 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			 tm.tm_hour, tm.tm_min, tm.tm_sec);
		REQUIRE(advss::IsValidEventSubTimestamp(buf));
	}
}

TEST_CASE("Expired timestamps are rejected", "[twitch][timestamp]")
{
	SECTION("11 minutes in the past (outside 10-minute window)")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			makeTimestamp(11 * 60)));
	}

	SECTION("1 hour in the past")
	{
		REQUIRE_FALSE(
			advss::IsValidEventSubTimestamp(makeTimestamp(3600)));
	}

	SECTION("2 minutes in the future (outside -1min allowance)")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			makeTimestamp(-2 * 60)));
	}
}

TEST_CASE("Invalid timestamp strings are rejected", "[twitch][timestamp]")
{
	SECTION("Empty string")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(""));
	}

	SECTION("Completely wrong format")
	{
		REQUIRE_FALSE(
			advss::IsValidEventSubTimestamp("not-a-timestamp"));
	}

	SECTION("Missing time component")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp("2023-07-19Z"));
	}

	SECTION("Missing UTC offset")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			"2023-07-19T14:56:51.634234626"));
	}

	SECTION("Malformed offset - missing minutes")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			"2023-07-19T14:56:51+05"));
	}

	SECTION("Malformed offset - non-numeric")
	{
		REQUIRE_FALSE(advss::IsValidEventSubTimestamp(
			"2023-07-19T14:56:51+XX:XX"));
	}

	SECTION("Truncated after seconds")
	{
		REQUIRE_FALSE(
			advss::IsValidEventSubTimestamp("2023-07-19T14:56:"));
	}
}
