#include "catch.hpp"
#include "macro-condition-process.hpp"

namespace advss {
void SetStubForegroundProcessName(const std::string &);
void SetStubForegroundProcessPath(const std::string &);
void SetStubProcessList(const QStringList &);
void SetStubProcessPaths(const QStringList &);
} // namespace advss

using advss::MacroConditionProcess;

// ---------------------------------------------------------------------------
// Name matching — no focus, no path
// ---------------------------------------------------------------------------

TEST_CASE("no focus: exact name match returns true",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"game.exe", "obs64.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = false;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("no focus: exact name mismatch returns false",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"obs64.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = false;

	REQUIRE_FALSE(cond.CheckCondition());
}

TEST_CASE("no focus: regex name match returns true",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"game.exe", "obs64.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.*";
	cond._checkFocus = false;
	cond._regex.SetEnabled(true);

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("no focus: regex name mismatch returns false",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"obs64.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.*";
	cond._checkFocus = false;
	cond._regex.SetEnabled(true);

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Focus — name only
// ---------------------------------------------------------------------------

TEST_CASE("focus: foreground name matches returns true",
	  "[macro-condition-process]")
{
	advss::SetStubForegroundProcessName("game.exe");
	advss::SetStubForegroundProcessPath("C:/Games/Game/game.exe");

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = true;

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("focus: foreground name does not match returns false",
	  "[macro-condition-process]")
{
	advss::SetStubForegroundProcessName("obs64.exe");
	advss::SetStubForegroundProcessPath("C:/OBS/obs64.exe");

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = true;

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Path matching — no focus
// ---------------------------------------------------------------------------

TEST_CASE("no focus with path: name and path both match returns true",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"game.exe"});
	advss::SetStubProcessPaths({"C:/Steam/steamapps/common/Game/game.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = false;
	cond._checkPath = true;
	cond._processPath = "C:/Steam/steamapps/common/Game/game.exe";

	REQUIRE(cond.CheckCondition());
}

TEST_CASE("no focus with path: name matches but path does not returns false",
	  "[macro-condition-process]")
{
	advss::SetStubProcessList({"game.exe"});
	advss::SetStubProcessPaths({"C:/Epic/Games/Game/game.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = false;
	cond._checkPath = true;
	cond._processPath = "Steam";

	REQUIRE_FALSE(cond.CheckCondition());
}

// ---------------------------------------------------------------------------
// Focus + path
//
// When both focus and path are checked, both must be satisfied by the *same*
// foreground process instance. A background process that matches the path
// but is not in focus must not cause a false positive.
// ---------------------------------------------------------------------------

TEST_CASE("focus with path: foreground matches both name and path returns true",
	  "[macro-condition-process]")
{
	advss::SetStubForegroundProcessName("game.exe");
	advss::SetStubForegroundProcessPath(
		"C:/Steam/steamapps/common/Game/game.exe");

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = true;
	cond._checkPath = true;
	cond._processPath = "C:/Steam/steamapps/common/Game/game.exe";

	REQUIRE(cond.CheckCondition());
}

TEST_CASE(
	"focus with path: foreground name matches but path does not returns false",
	"[macro-condition-process]")
{
	advss::SetStubForegroundProcessName("game.exe");
	advss::SetStubForegroundProcessPath("C:/Epic/Games/Game/game.exe");
	advss::SetStubProcessPaths({"C:/Steam/steamapps/common/Game/game.exe"});

	MacroConditionProcess cond(nullptr);
	cond._process = "game.exe";
	cond._checkFocus = true;
	cond._checkPath = true;
	cond._processPath = "C:/Steam/steamapps/common/Game/game.exe";

	REQUIRE_FALSE(cond.CheckCondition());
}
