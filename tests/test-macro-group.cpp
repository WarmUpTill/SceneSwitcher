#include "catch.hpp"
#include "macro-test-helpers.hpp"

#include <macro.hpp>
#include <macro-helpers.hpp>

#include <obs.hpp>

namespace {

using namespace advss;
using namespace advss::test;

using Names = std::vector<std::string>;

Names names(const std::vector<std::shared_ptr<Macro>> &macros)
{
	Names result;
	for (const auto &macro : macros) {
		result.emplace_back(macro->Name());
	}
	return result;
}

uint32_t groupSize(const std::string &name)
{
	auto group = GetMacro(name);
	REQUIRE(group);
	REQUIRE(group->IsGroup());
	return group->GroupSize();
}

void loadFromJson(const char *json)
{
	OBSDataAutoRelease data = obs_data_create_from_json(json);
	REQUIRE(data);
	LoadMacros(data);
}

void saveAndLoad()
{
	OBSDataAutoRelease data = obs_data_create();
	SaveMacros(data);
	GetTopLevelMacros().clear();
	LoadMacros(data);
}

struct ClearMacros {
	~ClearMacros() { GetTopLevelMacros().clear(); }
};

} // namespace

TEST_CASE("Nested group sizes", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	REQUIRE(DescribeMacros() == "A, G1[B, G2[C, D], E], F");
	REQUIRE(groupSize("G1") == 5);
	REQUIRE(groupSize("G2") == 2);
	REQUIRE(GetMacro("C")->Parent() == GetMacro("G2"));
	REQUIRE(GetMacro("G2")->Parent() == GetMacro("G1"));
	REQUIRE_FALSE(GetMacro("A")->Parent());
}

TEST_CASE("Group helpers", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E, G3[]], F");
	auto g1 = GetMacro("G1").get();
	auto g2 = GetMacro("G2").get();

	REQUIRE(names(GetGroupChildren(g1)) == Names{"B", "G2", "E", "G3"});
	REQUIRE(names(GetGroupDescendants(g1)) ==
		Names{"B", "G2", "C", "D", "E", "G3"});
	REQUIRE(names(GetGroupMacroEntries(g1)) == Names{"B", "C", "D", "E"});
	REQUIRE(names(GetGroupMacroEntries(g2)) == Names{"C", "D"});
	REQUIRE(GetGroupMacroEntries(GetMacro("G3").get()).empty());
	REQUIRE(GetGroupChildren(GetMacro("A").get()).empty());

	REQUIRE(IsMacroInGroup(GetMacro("C").get(), g1));
	REQUIRE(IsMacroInGroup(GetMacro("C").get(), g2));
	REQUIRE_FALSE(IsMacroInGroup(GetMacro("B").get(), g2));
	REQUIRE_FALSE(IsMacroInGroup(g1, g1));
	REQUIRE_FALSE(IsMacroInGroup(GetMacro("A").get(), g1));
}

TEST_CASE("Move macros between nested groups", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	SECTION("Group with children to top level")
	{
		Macro::PrepareMoveToGroup(nullptr, GetMacro("G2"));
		REQUIRE(groupSize("G1") == 2);
		REQUIRE(groupSize("G2") == 2);
		REQUIRE_FALSE(GetMacro("G2")->Parent());
	}

	SECTION("Macro into nested group")
	{
		Macro::PrepareMoveToGroup(GetMacro("G2"), GetMacro("A"));
		REQUIRE(groupSize("G1") == 6);
		REQUIRE(groupSize("G2") == 3);
		REQUIRE(GetMacro("A")->Parent() == GetMacro("G2"));
	}

	SECTION("Macro from nested group to outer group")
	{
		Macro::PrepareMoveToGroup(GetMacro("G1"), GetMacro("C"));
		REQUIRE(groupSize("G1") == 5);
		REQUIRE(groupSize("G2") == 1);
	}

	SECTION("Group into itself is rejected")
	{
		Macro::PrepareMoveToGroup(GetMacro("G1"), GetMacro("G1"));
		REQUIRE_FALSE(GetMacro("G1")->Parent());
		REQUIRE(groupSize("G1") == 5);
	}

	SECTION("Group into own descendant is rejected")
	{
		Macro::PrepareMoveToGroup(GetMacro("G2"), GetMacro("G1"));
		REQUIRE_FALSE(GetMacro("G1")->Parent());
		REQUIRE(groupSize("G1") == 5);
		REQUIRE(groupSize("G2") == 2);
	}
}

TEST_CASE("Create group from nested macros", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, C, D]");
	std::vector<std::shared_ptr<Macro>> children = {GetMacro("B"),
							GetMacro("C")};
	auto group = Macro::CreateGroup("G2", children);

	REQUIRE(group->Parent() == GetMacro("G1"));
	REQUIRE(group->GroupSize() == 2);
	REQUIRE(groupSize("G1") == 4);
	REQUIRE(GetMacro("B")->Parent() == group);
}

TEST_CASE("Remove nested group", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	SECTION("Inner group")
	{
		Macro::RemoveGroup(GetMacro("G2"));
		REQUIRE(DescribeMacros() == "A, G1[B, C, D, E], F");
		REQUIRE(groupSize("G1") == 4);
	}

	SECTION("Outer group")
	{
		Macro::RemoveGroup(GetMacro("G1"));
		REQUIRE(DescribeMacros() == "A, B, G2[C, D], E, F");
		REQUIRE(groupSize("G2") == 2);
	}
}

TEST_CASE("Save and load nested groups", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1{B, G2[C, G3{D}], E, G4[]}, F");
	saveAndLoad();

	REQUIRE(DescribeMacros() == "A, G1{B, G2[C, G3{D}], E, G4[]}, F");
	REQUIRE(groupSize("G1") == 7);
	REQUIRE(groupSize("G2") == 3);
	REQUIRE(groupSize("G3") == 1);
	REQUIRE(groupSize("G4") == 0);
}

TEST_CASE("Saved group size is number of descendants", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("G1[A, G2[B, C]]");
	OBSDataAutoRelease data = obs_data_create();
	SaveMacros(data);

	OBSDataArrayAutoRelease array = obs_data_get_array(data, "macros");
	REQUIRE(obs_data_array_count(array) == 5);
	OBSDataAutoRelease g1 = obs_data_array_item(array, 0);
	OBSDataAutoRelease g1Data = obs_data_get_obj(g1, "groupData");
	REQUIRE(obs_data_get_int(g1Data, "size") == 4);
	OBSDataAutoRelease g2 = obs_data_array_item(array, 2);
	OBSDataAutoRelease g2Data = obs_data_get_obj(g2, "groupData");
	REQUIRE(obs_data_get_int(g2Data, "size") == 2);
}

TEST_CASE("Load single level groups", "[macro-group]")
{
	ClearMacros clear;
	loadFromJson(R"({"macros": [
		{"name": "A"},
		{"name": "G1", "group": true,
		 "groupData": {"size": 2, "collapsed": true}},
		{"name": "B"},
		{"name": "C"},
		{"name": "D"}
	]})");

	REQUIRE(DescribeMacros() == "A, G1{B, C}, D");
	REQUIRE(groupSize("G1") == 2);
}

TEST_CASE("Load groups with invalid sizes", "[macro-group]")
{
	ClearMacros clear;

	SECTION("Group size exceeds number of macros")
	{
		loadFromJson(R"({"macros": [
			{"name": "A"},
			{"name": "G1", "group": true, "groupData": {"size": 5}},
			{"name": "B"},
			{"name": "C"}
		]})");
		REQUIRE(DescribeMacros() == "A, G1[B, C]");
		REQUIRE(groupSize("G1") == 2);
	}

	SECTION("Nested group exceeds parent group")
	{
		loadFromJson(R"({"macros": [
			{"name": "G1", "group": true, "groupData": {"size": 2}},
			{"name": "G2", "group": true, "groupData": {"size": 3}},
			{"name": "A"},
			{"name": "B"},
			{"name": "C"}
		]})");
		REQUIRE(DescribeMacros() == "G1[G2[A]], B, C");
		REQUIRE(groupSize("G1") == 2);
		REQUIRE(groupSize("G2") == 1);
	}

	SECTION("Empty group at the end")
	{
		loadFromJson(R"({"macros": [
			{"name": "A"},
			{"name": "G1", "group": true, "groupData": {"size": 1}}
		]})");
		REQUIRE(DescribeMacros() == "A, G1[]");
	}
}

TEST_CASE("Execution time is propagated to all parent groups", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("G1[G2[A]], B");
	const auto start = std::chrono::high_resolution_clock::now();
	GetMacro("A")->PerformActions(true);

	REQUIRE(GetMacro("G1")->WasExecutedSince(start));
	REQUIRE(GetMacro("G2")->WasExecutedSince(start));
	REQUIRE_FALSE(GetMacro("B")->WasExecutedSince(start));
}

namespace {

void checkGroupSizes()
{
	const auto &macros = GetTopLevelMacros();
	for (const auto &macro : macros) {
		if (!macro->IsGroup()) {
			continue;
		}
		INFO("group " << macro->Name() << " in " << DescribeMacros());
		const auto descendants = std::count_if(
			macros.begin(), macros.end(),
			[&macro](const std::shared_ptr<Macro> &m) {
				return IsMacroInGroup(m.get(), macro.get());
			});
		REQUIRE(macro->GroupSize() == (uint32_t)descendants);
	}
}

std::vector<std::shared_ptr<Macro>> macros(const Names &names)
{
	std::vector<std::shared_ptr<Macro>> result;
	for (const auto &name : names) {
		result.emplace_back(GetMacro(name));
	}
	return result;
}

int indexOf(const std::string &name)
{
	const auto &all = GetTopLevelMacros();
	for (int i = 0; i < (int)all.size(); i++) {
		if (all[i]->Name() == name) {
			return i;
		}
	}
	return -1;
}

// Mirrors AdvSceneSwitcher::ExportMacros()
OBSDataArrayAutoRelease exportMacros(const Names &selection)
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &macro :
	     GetMacrosWithGroupContents(macros(selection))) {
		OBSDataAutoRelease obj = obs_data_create();
		macro->Save(obj);
		obs_data_array_push_back(array, obj);
	}
	return array;
}

} // namespace

TEST_CASE("Outermost macros", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	REQUIRE(names(GetOutermostMacros(macros({"C", "G2", "F", "A"}))) ==
		Names{"A", "G2", "F"});
	REQUIRE(names(GetOutermostMacros(macros({"C", "G2", "G1"}))) ==
		Names{"G1"});
	REQUIRE(GetOutermostMacros({}).empty());
}

TEST_CASE("Macros with group contents", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	REQUIRE(names(GetMacrosWithGroupContents(macros({"F", "G2", "A"}))) ==
		Names{"A", "G2", "C", "D", "F"});
	REQUIRE(names(GetMacrosWithGroupContents(macros({"C", "G1"}))) ==
		Names{"G1", "B", "G2", "C", "D", "E"});
}

TEST_CASE("Remove and restore macro subtrees", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2[C, D], E], F");

	SECTION("Macro in nested group")
	{
		auto macro = GetMacro("C");
		const int index = indexOf("C");
		RemoveMacroSubtree(macro);
		REQUIRE(DescribeMacros() == "A, G1[B, G2[D], E], F");
		checkGroupSizes();

		InsertMacroSubtree(index, {macro}, GetMacro("G2"));
		REQUIRE(DescribeMacros() == "A, G1[B, G2[C, D], E], F");
		checkGroupSizes();
	}

	SECTION("Nested group")
	{
		const std::deque<std::shared_ptr<Macro>> subtree = {
			GetMacro("G2"), GetMacro("C"), GetMacro("D")};
		const int index = indexOf("G2");
		RemoveMacroSubtree(GetMacro("G2"));
		REQUIRE(DescribeMacros() == "A, G1[B, E], F");
		checkGroupSizes();

		InsertMacroSubtree(index, subtree, GetMacro("G1"));
		REQUIRE(DescribeMacros() == "A, G1[B, G2[C, D], E], F");
		checkGroupSizes();
	}

	// Mirrors the group delete undo, which saves and loads the group and
	// its contents
	SECTION("Group from saved data")
	{
		auto group = GetMacro("G1");
		const int index = indexOf("G1");
		OBSDataArrayAutoRelease array = exportMacros({"G1"});
		RemoveMacroSubtree(group);
		REQUIRE(DescribeMacros() == "A, F");

		std::deque<std::shared_ptr<Macro>> subtree;
		for (size_t i = 0; i < obs_data_array_count(array); i++) {
			OBSDataAutoRelease obj = obs_data_array_item(array, i);
			auto macro = std::make_shared<Macro>();
			macro->Load(obj);
			subtree.emplace_back(macro);
		}
		Macro::RestoreGroupHierarchy(subtree);
		InsertMacroSubtree(index, subtree, nullptr);
		REQUIRE(DescribeMacros() == "A, G1[B, G2[C, D], E], F");
		checkGroupSizes();
	}
}

TEST_CASE("Ungroup and regroup", "[macro-group]")
{
	ClearMacros clear;

	SECTION("Nested group")
	{
		SetupMacros("A, G1[B, G2[C, D], E], F");
		const int index = indexOf("G2");
		auto children = GetGroupChildren(GetMacro("G2").get());
		Macro::RemoveGroup(GetMacro("G2"));
		REQUIRE(DescribeMacros() == "A, G1[B, C, D, E], F");

		InsertGroup("G2", index, children, GetMacro("G1"));
		REQUIRE(DescribeMacros() == "A, G1[B, G2[C, D], E], F");
		checkGroupSizes();
	}

	SECTION("Group containing groups")
	{
		SetupMacros("A, G1[B, G2[C, G3[D]], E], F");
		const int index = indexOf("G1");
		auto children = GetGroupChildren(GetMacro("G1").get());
		Macro::RemoveGroup(GetMacro("G1"));
		REQUIRE(DescribeMacros() == "A, B, G2[C, G3[D]], E, F");

		InsertGroup("G1", index, children, nullptr);
		REQUIRE(DescribeMacros() == "A, G1[B, G2[C, G3[D]], E], F");
		checkGroupSizes();
	}

	SECTION("Empty nested group")
	{
		SetupMacros("A, G1[G2[]]");
		const int index = indexOf("G2");
		std::vector<std::shared_ptr<Macro>> children;
		Macro::RemoveGroup(GetMacro("G2"));
		REQUIRE(DescribeMacros() == "A, G1[]");

		InsertGroup("G2", index, children, GetMacro("G1"));
		REQUIRE(DescribeMacros() == "A, G1[G2[]]");
		checkGroupSizes();
	}
}

TEST_CASE("Export and import nested groups", "[macro-group]")
{
	ClearMacros clear;
	SetupMacros("A, G1[B, G2{C, D}], E");
	OBSDataArrayAutoRelease array = exportMacros({"G1", "C"});
	const auto importAll = [](const std::shared_ptr<Macro> &) {
		return true;
	};

	SECTION("Into empty macro list")
	{
		GetTopLevelMacros().clear();
		const auto imported = LoadImportedMacros(array, importAll);
		REQUIRE(names(imported) == Names{"G1", "B", "G2", "C", "D"});
		REQUIRE(DescribeMacros() == "G1[B, G2{C, D}]");
		checkGroupSizes();
	}

	// Mirrors AdvSceneSwitcher::ImportMacros() skipping macros for which
	// the name conflict was not resolved
	const auto renameOrSkip = [](const std::string &skip) {
		return [skip](const std::shared_ptr<Macro> &macro) {
			if (macro->Name() == skip) {
				return false;
			}
			macro->SetName(macro->Name() + "x");
			return true;
		};
	};

	SECTION("Skip outer group")
	{
		LoadImportedMacros(array, renameOrSkip("G1"));
		REQUIRE(DescribeMacros() ==
			"A, G1[B, G2{C, D}], E, Bx, G2x{Cx, Dx}");
		checkGroupSizes();
	}

	SECTION("Skip inner group")
	{
		LoadImportedMacros(array, renameOrSkip("G2"));
		REQUIRE(DescribeMacros() ==
			"A, G1[B, G2{C, D}], E, G1x[Bx, Cx, Dx]");
		checkGroupSizes();
	}

	SECTION("Skip macro in nested group")
	{
		LoadImportedMacros(array, renameOrSkip("C"));
		REQUIRE(DescribeMacros() ==
			"A, G1[B, G2{C, D}], E, G1x[Bx, G2x{Dx}]");
		checkGroupSizes();
	}
}

TEST_CASE("Import single level groups", "[macro-group]")
{
	ClearMacros clear;
	OBSDataAutoRelease data = obs_data_create_from_json(R"({"macros": [
		{"name": "G1", "group": true,
		 "groupData": {"size": 2, "collapsed": true}},
		{"name": "A"},
		{"name": "B"},
		{"name": "C"}
	]})");
	OBSDataArrayAutoRelease array = obs_data_get_array(data, "macros");
	LoadImportedMacros(array,
			   [](const std::shared_ptr<Macro> &) { return true; });
	REQUIRE(DescribeMacros() == "G1{A, B}, C");
	checkGroupSizes();
}
