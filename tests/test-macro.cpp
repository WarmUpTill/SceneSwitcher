#include "catch.hpp"

#include <macro.hpp>

#include <memory>

namespace {

// ---------------------------------------------------------------------------
// Stubs
// ---------------------------------------------------------------------------

class StubCondition : public advss::MacroCondition {
public:
	StubCondition(advss::Macro *m, bool initialValue = false)
		: MacroCondition(m),
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

class StubAction : public advss::MacroAction {
public:
	StubAction(advss::Macro *m) : MacroAction(m) {}

	bool PerformAction() override
	{
		_performCount++;
		return true;
	}

	std::shared_ptr<MacroAction> Copy() const override
	{
		return std::make_shared<StubAction>(*this);
	}

	bool Save(obs_data_t *) const override { return true; }
	bool Load(obs_data_t *) override { return true; }
	std::string GetId() const override { return "stub"; }

	int PerformCount() const { return _performCount; }

private:
	int _performCount = 0;
};

// Helpers to wire up conditions and actions onto a macro
std::shared_ptr<StubCondition> AddCondition(advss::Macro &m,
					    bool initialValue = false)
{
	auto cond = std::make_shared<StubCondition>(&m, initialValue);
	cond->SetLogicType(advss::Logic::Type::ROOT_NONE);
	m.Conditions().push_back(cond);
	return cond;
}

std::shared_ptr<StubAction> AddAction(advss::Macro &m)
{
	auto action = std::make_shared<StubAction>(&m);
	m.Actions().push_back(action);
	return action;
}

std::shared_ptr<StubAction> AddElseAction(advss::Macro &m)
{
	auto action = std::make_shared<StubAction>(&m);
	m.ElseActions().push_back(action);
	return action;
}

} // namespace

// ---------------------------------------------------------------------------
// CheckConditions - basic matching
// ---------------------------------------------------------------------------

TEST_CASE("CheckConditions returns false with no conditions", "[macro]")
{
	advss::Macro m("test");
	REQUIRE_FALSE(m.CheckConditions());
}

TEST_CASE("CheckConditions returns true when condition is true", "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	REQUIRE(m.CheckConditions());
}

TEST_CASE("CheckConditions returns false when condition is false", "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, false);
	REQUIRE_FALSE(m.CheckConditions());
}

TEST_CASE("ConditionsMatched reflects last CheckConditions result", "[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, false);

	m.CheckConditions();
	REQUIRE_FALSE(m.ConditionsMatched());

	cond->SetValue(true);
	m.CheckConditions();
	REQUIRE(m.ConditionsMatched());
}

TEST_CASE("CheckConditions returns false for group macros", "[macro]")
{
	std::vector<std::shared_ptr<advss::Macro>> children;
	auto group = advss::Macro::CreateGroup("group", children);
	REQUIRE_FALSE(group->CheckConditions());
}

// ---------------------------------------------------------------------------
// Pause
// ---------------------------------------------------------------------------

TEST_CASE("CheckConditions respects pause", "[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, true);

	m.SetPaused(true);
	// When paused, conditions are not evaluated — _matched stays false
	REQUIRE_FALSE(m.CheckConditions());
}

TEST_CASE("CheckConditions runs when ignorePause is true while paused",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);

	m.SetPaused(true);
	REQUIRE(m.CheckConditions(/*ignorePause=*/true));
}

// ---------------------------------------------------------------------------
// ShouldRunActions - ActionTriggerMode::MACRO_RESULT_CHANGED (default)
// ---------------------------------------------------------------------------

TEST_CASE("ShouldRunActions is false before first CheckConditions", "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);
	// _actionModeMatch starts false, _matched starts false
	REQUIRE_FALSE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions MACRO_RESULT_CHANGED: true on first match",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	// Default mode is MACRO_RESULT_CHANGED.
	// _lastMatched starts false, first check sets _matched=true -> changed
	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::MACRO_RESULT_CHANGED);
	m.CheckConditions();
	REQUIRE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions MACRO_RESULT_CHANGED: false when result unchanged",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::MACRO_RESULT_CHANGED);
	m.CheckConditions(); // false -> true: changed
	m.CheckConditions(); // true -> true: unchanged
	REQUIRE_FALSE(m.ShouldRunActions());
}

TEST_CASE(
	"ShouldRunActions MACRO_RESULT_CHANGED: true again when result flips back",
	"[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, true);
	AddAction(m);
	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::MACRO_RESULT_CHANGED);
	m.CheckConditions(); // false -> true
	REQUIRE(m.ShouldRunActions());
	m.CheckConditions(); // true -> true
	REQUIRE_FALSE(m.ShouldRunActions());
	cond->SetValue(false);
	m.CheckConditions(); // true -> false: changed
	cond->SetValue(true);
	m.CheckConditions(); // true -> false: changed
	REQUIRE(m.ShouldRunActions());
}

// ---------------------------------------------------------------------------
// ShouldRunActions - ActionTriggerMode::ALWAYS
// ---------------------------------------------------------------------------

TEST_CASE("ShouldRunActions ALWAYS: true every time conditions match",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(advss::Macro::ActionTriggerMode::ALWAYS);
	m.CheckConditions();
	REQUIRE(m.ShouldRunActions());

	m.CheckConditions();
	REQUIRE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions ALWAYS: false when conditions do not match",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, false);
	// No else actions, so nothing to run
	m.SetActionTriggerMode(advss::Macro::ActionTriggerMode::ALWAYS);
	m.CheckConditions();
	REQUIRE_FALSE(m.ShouldRunActions());
}

// ---------------------------------------------------------------------------
// ShouldRunActions - ActionTriggerMode::ANY_CONDITION_CHANGED
// ---------------------------------------------------------------------------

TEST_CASE("ShouldRunActions ANY_CONDITION_CHANGED: true when condition changes",
	  "[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, false);
	AddAction(m);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_CHANGED);
	m.CheckConditions(); // baseline — no change yet

	cond->SetValue(true);
	m.CheckConditions(); // changed: false -> true
	REQUIRE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions ANY_CONDITION_CHANGED: false when condition stable",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_CHANGED);
	m.CheckConditions(); // baseline
	m.CheckConditions(); // stable
	REQUIRE_FALSE(m.ShouldRunActions());
}

// ---------------------------------------------------------------------------
// ShouldRunActions - ActionTriggerMode::ANY_CONDITION_TRIGGERED
// ---------------------------------------------------------------------------

TEST_CASE("ShouldRunActions ANY_CONDITION_TRIGGERED: true on rising edge",
	  "[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, false);
	AddAction(m);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_TRIGGERED);
	m.CheckConditions(); // baseline

	cond->SetValue(true);
	m.CheckConditions(); // rising edge
	REQUIRE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions ANY_CONDITION_TRIGGERED: false on falling edge",
	  "[macro]")
{
	advss::Macro m("test");
	auto cond = AddCondition(m, true);
	AddElseAction(m); // need else actions so ShouldRunActions can be true

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_TRIGGERED);
	m.CheckConditions(); // baseline (first eval, no rising edge)

	cond->SetValue(false);
	m.CheckConditions(); // falling edge — not a rising edge
	REQUIRE_FALSE(m.ShouldRunActions());
}

TEST_CASE("ShouldRunActions ANY_CONDITION_TRIGGERED: false when stable true",
	  "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_TRIGGERED);
	m.CheckConditions(); // baseline
	m.CheckConditions(); // stable true — no rising edge
	REQUIRE_FALSE(m.ShouldRunActions());
}

// ---------------------------------------------------------------------------
// RunCount
// ---------------------------------------------------------------------------

TEST_CASE("RunCount increments after PerformActions", "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(advss::Macro::ActionTriggerMode::ALWAYS);
	m.CheckConditions();
	REQUIRE(m.RunCount() == 0);

	m.PerformActions(true);
	REQUIRE(m.RunCount() == 1);

	m.PerformActions(true);
	REQUIRE(m.RunCount() == 2);
}

TEST_CASE("ResetRunCount resets to zero", "[macro]")
{
	advss::Macro m("test");
	AddAction(m);

	m.PerformActions(true);
	m.PerformActions(true);
	REQUIRE(m.RunCount() == 2);

	m.ResetRunCount();
	REQUIRE(m.RunCount() == 0);
}

// ---------------------------------------------------------------------------
// Pause state
// ---------------------------------------------------------------------------

TEST_CASE("Paused is false by default", "[macro]")
{
	advss::Macro m("test");
	REQUIRE_FALSE(m.Paused());
}

TEST_CASE("SetPaused toggles pause state", "[macro]")
{
	advss::Macro m("test");
	m.SetPaused(true);
	REQUIRE(m.Paused());
	m.SetPaused(false);
	REQUIRE_FALSE(m.Paused());
}

TEST_CASE("Paused macros don't run actions", "[macro]")
{
	advss::Macro m("test");
	m.SetPaused(true);
	REQUIRE(m.Paused());
	auto action = AddAction(m);
	m.PerformActions(true);
	REQUIRE(action->PerformCount() == 0);
}

TEST_CASE("ShouldRunActions is false while paused", "[macro]")
{
	advss::Macro m("test");
	AddCondition(m, true);
	AddAction(m);

	m.SetActionTriggerMode(advss::Macro::ActionTriggerMode::ALWAYS);
	m.CheckConditions(/*ignorePause=*/true);
	m.SetPaused(true);
	REQUIRE_FALSE(m.ShouldRunActions());
}

// ---------------------------------------------------------------------------
// ActionTriggerMode getter/setter
// ---------------------------------------------------------------------------

TEST_CASE("GetActionTriggerMode returns the set mode", "[macro]")
{
	advss::Macro m("test");
	m.SetActionTriggerMode(advss::Macro::ActionTriggerMode::ALWAYS);
	REQUIRE(m.GetActionTriggerMode() ==
		advss::Macro::ActionTriggerMode::ALWAYS);

	m.SetActionTriggerMode(
		advss::Macro::ActionTriggerMode::ANY_CONDITION_TRIGGERED);
	REQUIRE(m.GetActionTriggerMode() ==
		advss::Macro::ActionTriggerMode::ANY_CONDITION_TRIGGERED);
}
