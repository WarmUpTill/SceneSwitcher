#include "catch.hpp"
#include "macro-condition-window.hpp"
#include "platform-funcs.hpp"

namespace advss {
void SetStubWindows(const std::vector<WindowInfo> &);
void SetStubForegroundWindowTitle(const std::string &);
void SetStubPreviousForegroundWindowTitle(const std::string &);
} // namespace advss

using advss::MacroConditionWindow;
using advss::WindowInfo;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static WindowInfo MakeWindow(const std::string &title, bool focused = false,
			     bool fullscreen = false, bool maximized = false)
{
	WindowInfo info;
	info.title = title;
	info.focused = focused;
	info.fullscreen = fullscreen;
	info.maximized = maximized;
	info.x = 0;
	info.y = 0;
	info.width = 1920;
	info.height = 1080;
	return info;
}

// ---------------------------------------------------------------------------
// Title matching
// ---------------------------------------------------------------------------

TEST_CASE("title: exact match returns true", "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game")});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("title: mismatch returns false", "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("OtherApp")});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("title: check disabled matches any window",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("SomeRandomApp")});

	MacroConditionWindow cond(nullptr);
	cond._checkTitle = false;
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("title: empty window list returns false", "[macro-condition-window]")
{
	advss::SetStubWindows({});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("title: first matching window from list is selected",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("OBS"), MakeWindow("Game"), MakeWindow("Browser")});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Regex title matching
// ---------------------------------------------------------------------------

TEST_CASE("regex: matching pattern returns true", "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("My Game v2.1")});

	MacroConditionWindow cond(nullptr);
	cond._window = "My Game.*";
	cond._windowRegex.SetEnabled(true);
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("regex: non-matching pattern returns false",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("OBS Studio")});

	MacroConditionWindow cond(nullptr);
	cond._window = "My Game.*";
	cond._windowRegex.SetEnabled(true);
	cond._focus = false;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("regex: matches second window in list", "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("OBS Studio"), MakeWindow("Game 2")});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game.*";
	cond._windowRegex.SetEnabled(true);
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Focus check
// ---------------------------------------------------------------------------

TEST_CASE("focus: focused window matches when focus required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game", /*focused=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("focus: unfocused window fails when focus required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game", /*focused=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("focus: unfocused window still matches when focus not required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game", /*focused=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("focus: multiple windows, only focused one satisfies requirement",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game", /*focused=*/false),
			       MakeWindow("Game", /*focused=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Fullscreen check
// ---------------------------------------------------------------------------

TEST_CASE("fullscreen: fullscreen window matches when fullscreen required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game", false, /*fullscreen=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._fullscreen = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("fullscreen: non-fullscreen window fails when fullscreen required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", false, /*fullscreen=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._fullscreen = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE(
	"fullscreen: non-fullscreen window matches when fullscreen not required",
	"[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", false, /*fullscreen=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._fullscreen = false;

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Maximized check
// ---------------------------------------------------------------------------

TEST_CASE("maximized: maximized window matches when maximized required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", false, false, /*maximized=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._maximized = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("maximized: non-maximized window fails when maximized required",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", false, false, /*maximized=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._maximized = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Combined checks
// ---------------------------------------------------------------------------

TEST_CASE("combined: title + focus + fullscreen all satisfied returns true",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", /*focused=*/true, /*fullscreen=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;
	cond._fullscreen = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("combined: title matches but focus fails returns false",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", /*focused=*/false, /*fullscreen=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;
	cond._fullscreen = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("combined: title matches but fullscreen fails returns false",
	  "[macro-condition-window]")
{
	advss::SetStubWindows(
		{MakeWindow("Game", /*focused=*/true, /*fullscreen=*/false)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;
	cond._fullscreen = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("combined: second window satisfies all requirements",
	  "[macro-condition-window]")
{
	// First window has wrong title; second matches everything
	advss::SetStubWindows(
		{MakeWindow("OBS", true, true, true),
		 MakeWindow("Game", /*focused=*/true, /*fullscreen=*/true,
			    /*maximized=*/true)});

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = true;
	cond._fullscreen = true;
	cond._maximized = true;

	REQUIRE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Window focus changed
// ---------------------------------------------------------------------------

TEST_CASE("windowFocusChanged: foreground changed and match returns true",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game")});
	advss::SetStubForegroundWindowTitle("Game");
	advss::SetStubPreviousForegroundWindowTitle("OtherApp");

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._windowFocusChanged = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("windowFocusChanged: foreground unchanged returns false",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game")});
	advss::SetStubForegroundWindowTitle("Game");
	advss::SetStubPreviousForegroundWindowTitle("Game");

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._windowFocusChanged = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("windowFocusChanged: window mismatch returns false even if changed",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("OBS")});
	advss::SetStubForegroundWindowTitle("OBS");
	advss::SetStubPreviousForegroundWindowTitle("Game");

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._windowFocusChanged = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("windowFocusChanged: disabled, foreground same, title matches -> true",
	  "[macro-condition-window]")
{
	advss::SetStubWindows({MakeWindow("Game")});
	advss::SetStubForegroundWindowTitle("Game");
	advss::SetStubPreviousForegroundWindowTitle("Game");

	MacroConditionWindow cond(nullptr);
	cond._window = "Game";
	cond._checkTitle = true;
	cond._focus = false;
	cond._windowFocusChanged = false;

	REQUIRE(cond.CheckCondition());
}
