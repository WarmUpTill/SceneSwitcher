#include "catch.hpp"

#include <duration-modifier.hpp>

#include <chrono>
#include <thread>

TEST_CASE("Test modifier NONE", "[duration-modifier]")
{
	advss::DurationModifier durationModifier;
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));

	// Duration should not affect the condition outcome with Type::NONE
	durationModifier.SetDuration(1.0);

	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));
}

TEST_CASE("Test modifier MORE", "[duration-modifier]")
{
	using namespace std::chrono_literals;
	advss::DurationModifier durationModifier;
	durationModifier.SetModifier(advss::DurationModifier::Type::MORE);
	durationModifier.SetDuration(.1);
	durationModifier.ResetDuration();

	// Should only return true after 100ms have passed
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
	std::this_thread::sleep_for(50ms);
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
	std::this_thread::sleep_for(100ms);
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));

	// Should return false for at 100ms as soon as "false" is passed
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
	std::this_thread::sleep_for(50ms);
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
	std::this_thread::sleep_for(100ms);
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
}

TEST_CASE("Test modifier EQUAL", "[duration-modifier]")
{
	using namespace std::chrono_literals;
	advss::DurationModifier durationModifier;
	durationModifier.SetModifier(advss::DurationModifier::Type::EQUAL);
	durationModifier.SetDuration(.1);
	durationModifier.ResetDuration();

	// Return false value if duration is not reached
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));

	std::this_thread::sleep_for(50ms);
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));

	// Should return true only once after duration
	std::this_thread::sleep_for(100ms);
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));

	// Should reset to original behavior as soon as false is passed after
	// duration was reached
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));
}

TEST_CASE("Test modifier LESS", "[duration-modifier]")
{
	using namespace std::chrono_literals;
	advss::DurationModifier durationModifier;
	durationModifier.SetModifier(advss::DurationModifier::Type::LESS);
	durationModifier.SetDuration(.1);
	durationModifier.ResetDuration();

	// Return input value if duration is not reached
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));

	std::this_thread::sleep_for(50ms);
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));

	// Should return false after duration is reached even if true is passed
	std::this_thread::sleep_for(100ms);
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(true));

	// Should reset to original behavior as soon as false is passed after
	// duration was reached
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
}

TEST_CASE("Test modifier WITHIN", "[duration-modifier]")
{
	using namespace std::chrono_literals;
	advss::DurationModifier durationModifier;
	durationModifier.SetModifier(advss::DurationModifier::Type::WITHIN);
	durationModifier.SetDuration(.1);
	durationModifier.ResetDuration();

	// Return false value if true was not passed in given time frame
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));

	// Passing true should always return true
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));

	// Passing true or false when true was passed at least once in the given
	// time frame should always return true
	std::this_thread::sleep_for(50ms);
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(false));

	// Should reset to original behavior if true was not passed within the
	// given time frame
	std::this_thread::sleep_for(150ms);
	REQUIRE_FALSE(
		durationModifier.CheckConditionWithDurationModifier(false));
	REQUIRE(durationModifier.CheckConditionWithDurationModifier(true));
}
