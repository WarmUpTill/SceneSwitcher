#include "catch.hpp"

#include <variable-ref.hpp>
#include <variable.hpp>

#include <obs.hpp>
#include <thread>

TEST_CASE("Variable", "[variable]")
{
	advss::Variable variable;
	REQUIRE_FALSE(variable.GetSecondsSinceLastUse());

	REQUIRE(variable.Value() == "");
	REQUIRE(variable.GetPreviousValue() == "");
	REQUIRE(variable.GetDefaultValue() == "");
	REQUIRE(variable.GetSaveAction() ==
		advss::Variable::SaveAction::DONT_SAVE);
	REQUIRE(variable.GetValueChangeCount() == 0);

	variable.SetValue("testing");
	REQUIRE(variable.Value() == "testing");
	REQUIRE(variable.GetPreviousValue() == "");
	REQUIRE(variable.GetValueChangeCount() == 1);

	variable.SetValue(123);
	REQUIRE(variable.Value() == "123");
	REQUIRE(variable.GetPreviousValue() == "testing");

	variable.SetValue(123.0);
	REQUIRE(variable.Value() == "123");

	variable.SetValue(123.123);
	REQUIRE(variable.Value() == "123.123");

	REQUIRE(*variable.GetSecondsSinceLastUse() == 0);
	REQUIRE(*variable.GetSecondsSinceLastChange() == 0);

	std::this_thread::sleep_for(std::chrono::seconds(2));
	REQUIRE(*variable.GetSecondsSinceLastUse() > 1);
	REQUIRE(*variable.GetSecondsSinceLastChange() > 1);

	variable.Value(false);
	REQUIRE(*variable.GetSecondsSinceLastUse() > 1);

	variable.MarkAsUsed();
	REQUIRE(*variable.GetSecondsSinceLastUse() == 0);

	variable.SetValue(123);
	REQUIRE(*variable.GetSecondsSinceLastChange() == 0);

	std::this_thread::sleep_for(std::chrono::seconds(1));
	variable.SetValue(123);
	REQUIRE(*variable.GetSecondsSinceLastChange() > 0);
}

static std::shared_ptr<advss::Variable> addVariable(const std::string &name)
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "name", name.c_str());
	auto variable = std::make_shared<advss::Variable>();
	variable->Load(data);
	advss::GetVariables().emplace_back(variable);
	return variable;
}

TEST_CASE("VariableRef", "[variable]")
{
	auto var = addVariable("var");
	addVariable("var2");
	addVariable("Var");

	SECTION("Save reports and writes name")
	{
		advss::VariableRefCollector collector;
		advss::VariableRef ref(var);
		OBSDataAutoRelease data = obs_data_create();
		ref.Save(data, "key");
		REQUIRE(std::string(obs_data_get_string(data, "key")) == "var");
		REQUIRE(collector.Counts().at("var") == 1);

		advss::VariableRef loaded;
		loaded.Load(data, "key");
		REQUIRE(loaded.lock() == var);
	}

	SECTION("Expired ref is not saved or reported")
	{
		advss::VariableRefCollector collector;
		advss::VariableRef ref;
		OBSDataAutoRelease data = obs_data_create();
		ref.Save(data, "key");
		REQUIRE_FALSE(obs_data_has_user_value(data, "key"));
		REQUIRE(collector.Counts().empty());
	}

	SECTION("Text references")
	{
		advss::VariableRefCollector collector;
		advss::ReportVariableRefsInText(
			"${var} ${var2} ${var} var Var");
		const auto &counts = collector.Counts();
		REQUIRE(counts.at("var") == 2);
		REQUIRE(counts.at("var2") == 1);
		REQUIRE(counts.find("Var") == counts.end());
	}

	SECTION("Nested collectors")
	{
		advss::VariableRefCollector outer;
		{
			advss::VariableRefCollector inner;
			advss::ReportVariableRef(var);
			REQUIRE(inner.Counts().at("var") == 1);
		}
		REQUIRE(outer.Counts().empty());
		advss::ReportVariableRef(var);
		REQUIRE(outer.Counts().at("var") == 1);
	}

	advss::GetVariables().clear();
}
