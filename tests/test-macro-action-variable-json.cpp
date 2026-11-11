#include "catch.hpp"
#include "macro-action-variable-json.hpp"

#include <obs.hpp>

using advss::MacroActionVariable;
using advss::Variable;
using advss::VariableActionJson;
using Action = MacroActionVariable::Action;

static std::shared_ptr<Variable> addVariable(const std::string &name)
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "name", name.c_str());
	auto variable = std::make_shared<Variable>();
	variable->Load(data);
	advss::GetVariables().emplace_back(variable);
	return variable;
}

TEST_CASE("JSON variable actions read input and write output",
	  "[macro-action-variable]")
{
	auto output = addVariable("output");
	auto input = addVariable("input");
	MacroActionVariable action(nullptr);
	action._variable = std::weak_ptr<Variable>(output);
	action._variable2 = std::weak_ptr<Variable>(input);

	SECTION("Extract field")
	{
		const std::string json = R"({"field":"value"})";
		input->SetValue(json);
		action._action = Action::EXTRACT_JSON;
		action._strValue = "field";
		action.PerformAction();
		REQUIRE(output->Value() == "value");
		REQUIRE(input->Value() == json);
	}
	SECTION("Access array")
	{
		const std::string json = R"(["a","b"])";
		input->SetValue(json);
		action._action = Action::ARRAY_JSON;
		action.GetGroup<VariableActionJson>()->_jsonIndex = 1;
		action.PerformAction();
		REQUIRE(output->Value() == "b");
		REQUIRE(input->Value() == json);
	}
#ifdef JSONPATH_SUPPORT
	SECTION("Query")
	{
		const std::string json = R"({"field":"value"})";
		input->SetValue(json);
		action._action = Action::QUERY_JSON;
		action.GetGroup<VariableActionJson>()->_jsonQuery = "$.field";
		action.PerformAction();
		REQUIRE(output->Value() == "value");
		REQUIRE(input->Value() == json);
	}
#endif
	SECTION("Output only changes when the result changes")
	{
		input->SetValue(R"({"field":"value","other":1})");
		action._action = Action::EXTRACT_JSON;
		action._strValue = "field";
		action.PerformAction();
		const int changeCount = output->GetValueChangeCount();

		input->SetValue(R"({"field":"value","other":2})");
		action.PerformAction();
		REQUIRE(output->GetValueChangeCount() == changeCount);

		input->SetValue(R"({"field":"new","other":2})");
		action.PerformAction();
		REQUIRE(output->GetValueChangeCount() == changeCount + 1);
	}
	SECTION("Invalid input leaves output unchanged")
	{
		output->SetValue("unchanged");
		input->SetValue("not json");
		action._action = Action::EXTRACT_JSON;
		action._strValue = "field";
		action.PerformAction();
		REQUIRE(output->Value() == "unchanged");
	}
	SECTION("No input selected leaves output unchanged")
	{
		output->SetValue("unchanged");
		action._variable2 = std::weak_ptr<Variable>();
		action._action = Action::EXTRACT_JSON;
		action._strValue = "field";
		action.PerformAction();
		REQUIRE(output->Value() == "unchanged");
	}

	advss::GetVariables().clear();
}
