#include "catch.hpp"

#include <macro-condition.hpp>

namespace {

class StubCondition : public advss::MacroCondition {
public:
	StubCondition(bool initialValue = false)
		: MacroCondition(nullptr),
		  _value(initialValue)
	{
	}

	void SetValue(bool value) { _value = value; }
	bool CheckCondition() override { return _value; }
	bool Save(obs_data_t *) const override { return true; }
	bool Load(obs_data_t *) override { return true; }
	std::string GetId() const override { return "stub"; }

private:
	bool _value;
};

} // namespace

// ---------------------------------------------------------------------------
// HasChanged
// ---------------------------------------------------------------------------

TEST_CASE("HasChanged is false on first evaluation", "[macro-condition]")
{
	StubCondition cond;
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.HasChanged());
}

TEST_CASE("HasChanged is false when value stays false", "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.HasChanged());
}

TEST_CASE("HasChanged is false when value stays true", "[macro-condition]")
{
	StubCondition cond(true);
	cond.EvaluateCondition();
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.HasChanged());
}

TEST_CASE("HasChanged is true on rising edge (false to true)",
	  "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();
	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged());
}

TEST_CASE("HasChanged is true on falling edge (true to false)",
	  "[macro-condition]")
{
	StubCondition cond(true);
	cond.EvaluateCondition();
	cond.SetValue(false);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged());
}

TEST_CASE("HasChanged resets to false after stable evaluation",
	  "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged());

	cond.EvaluateCondition(); // value unchanged
	REQUIRE_FALSE(cond.HasChanged());
}

TEST_CASE("EvaluateCondition returns the current condition value",
	  "[macro-condition]")
{
	StubCondition cond(false);
	REQUIRE_FALSE(cond.EvaluateCondition());

	cond.SetValue(true);
	REQUIRE(cond.EvaluateCondition());

	cond.SetValue(false);
	REQUIRE_FALSE(cond.EvaluateCondition());
}

TEST_CASE("Multiple alternating changes are each detected", "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition(); // baseline

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged()); // false -> true

	cond.SetValue(false);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged()); // true -> false

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.HasChanged()); // false -> true again
}

// ---------------------------------------------------------------------------
// IsRisingEdge
// ---------------------------------------------------------------------------

TEST_CASE("IsRisingEdge is false on first evaluation", "[macro-condition]")
{
	StubCondition cond(true);
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge is true on false-to-true transition",
	  "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();
	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge is false on true-to-false transition",
	  "[macro-condition]")
{
	StubCondition cond(true);
	cond.EvaluateCondition();
	cond.SetValue(false);
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge is false when value stays true", "[macro-condition]")
{
	StubCondition cond(true);
	cond.EvaluateCondition();
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge is false when value stays false", "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge resets to false after stable evaluation",
	  "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.IsRisingEdge());

	cond.EvaluateCondition(); // value unchanged
	REQUIRE_FALSE(cond.IsRisingEdge());
}

TEST_CASE("IsRisingEdge fires again after falling then rising",
	  "[macro-condition]")
{
	StubCondition cond(false);
	cond.EvaluateCondition();

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.IsRisingEdge());

	cond.SetValue(false);
	cond.EvaluateCondition();
	REQUIRE_FALSE(cond.IsRisingEdge());

	cond.SetValue(true);
	cond.EvaluateCondition();
	REQUIRE(cond.IsRisingEdge()); // second rising edge is detected
}

// ---------------------------------------------------------------------------
// CheckDurationModifier - NONE (default, no time constraint)
// ---------------------------------------------------------------------------

TEST_CASE("DurationModifier NONE passes through condition value",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	// Default modifier type is NONE
	REQUIRE(cond.CheckDurationModifier(true));
	REQUIRE_FALSE(cond.CheckDurationModifier(false));
}

// ---------------------------------------------------------------------------
// CheckDurationModifier - MORE (true only after duration has elapsed)
// ---------------------------------------------------------------------------

TEST_CASE("DurationModifier MORE returns false before duration elapses",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::MORE);
	cond.SetDuration(advss::Duration(10.0)); // 10 seconds — won't elapse

	REQUIRE_FALSE(cond.CheckDurationModifier(true));
}

TEST_CASE("DurationModifier MORE returns true after duration elapses",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::MORE);
	cond.SetDuration(
		advss::Duration(0.0)); // 0 seconds — elapses immediately

	// First call starts the timer; with 0s duration it should already pass
	REQUIRE(cond.CheckDurationModifier(true));
}

TEST_CASE("DurationModifier MORE resets when condition becomes false",
	  "[duration-modifier]")
{
	StubCondition cond;
	cond.SetDurationModifier(advss::DurationModifier::Type::MORE);
	cond.SetDuration(advss::Duration(0.0));

	cond.CheckDurationModifier(true);  // start timer
	cond.CheckDurationModifier(false); // reset
	// After reset a fresh call with true must re-start the timer and still pass
	// (duration is 0 so it should pass immediately again)
	REQUIRE(cond.CheckDurationModifier(true));
}

TEST_CASE("DurationModifier MORE returns false when condition is false",
	  "[duration-modifier]")
{
	StubCondition cond;
	cond.SetDurationModifier(advss::DurationModifier::Type::MORE);
	cond.SetDuration(advss::Duration(0.0));

	REQUIRE_FALSE(cond.CheckDurationModifier(false));
}

// ---------------------------------------------------------------------------
// CheckDurationModifier - LESS (true only before duration elapses)
// ---------------------------------------------------------------------------

TEST_CASE("DurationModifier LESS returns true before duration elapses",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::LESS);
	cond.SetDuration(advss::Duration(10.0)); // won't elapse during test

	REQUIRE(cond.CheckDurationModifier(true));
}

TEST_CASE("DurationModifier LESS returns false after duration elapses",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::LESS);
	cond.SetDuration(advss::Duration(0.0)); // elapses immediately

	// First call starts timer; with 0s it is already past
	REQUIRE_FALSE(cond.CheckDurationModifier(true));
}

TEST_CASE("DurationModifier LESS returns false when condition is false",
	  "[duration-modifier]")
{
	StubCondition cond;
	cond.SetDurationModifier(advss::DurationModifier::Type::LESS);
	cond.SetDuration(advss::Duration(10.0));

	REQUIRE_FALSE(cond.CheckDurationModifier(false));
}

// ---------------------------------------------------------------------------
// CheckDurationModifier - WITHIN (true while within window after going false)
// ---------------------------------------------------------------------------

TEST_CASE("DurationModifier WITHIN returns true while condition is true",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::WITHIN);
	cond.SetDuration(advss::Duration(10.0));

	REQUIRE(cond.CheckDurationModifier(true));
}

TEST_CASE(
	"DurationModifier WITHIN returns true immediately after condition goes false",
	"[duration-modifier]")
{
	StubCondition cond;
	cond.SetDurationModifier(advss::DurationModifier::Type::WITHIN);
	cond.SetDuration(advss::Duration(10.0));

	cond.CheckDurationModifier(
		true); // condition was true — sets time remaining
	REQUIRE(cond.CheckDurationModifier(false)); // still within window
}

TEST_CASE("DurationModifier WITHIN returns false before condition was ever true",
	  "[duration-modifier]")
{
	StubCondition cond;
	cond.SetDurationModifier(advss::DurationModifier::Type::WITHIN);
	cond.SetDuration(advss::Duration(10.0));

	// Timer was never started (condition never went true->false)
	REQUIRE_FALSE(cond.CheckDurationModifier(false));
}

// ---------------------------------------------------------------------------
// ResetDuration
// ---------------------------------------------------------------------------

TEST_CASE("ResetDuration causes MORE modifier to restart its timer",
	  "[duration-modifier]")
{
	StubCondition cond(true);
	cond.SetDurationModifier(advss::DurationModifier::Type::MORE);
	cond.SetDuration(advss::Duration(10.0));

	cond.CheckDurationModifier(true); // starts timer
	cond.ResetDuration();             // reset

	// After reset the timer restarts — 10s duration won't have elapsed
	REQUIRE_FALSE(cond.CheckDurationModifier(true));
}
