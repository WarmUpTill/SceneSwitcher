#include "headers/macro-entry.hpp"

#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"

std::unordered_map<LogicType, LogicTypeInfo> MacroCondition::logicTypes = {
	{LogicType::NONE, {"AdvSceneSwitcher.logic.none"}},
	{LogicType::AND, {"AdvSceneSwitcher.logic.and"}},
	{LogicType::OR, {"AdvSceneSwitcher.logic.or"}},
	{LogicType::AND_NOT, {"AdvSceneSwitcher.logic.andNot"}},
	{LogicType::OR_NOT, {"AdvSceneSwitcher.logic.orNot"}},
};

std::unordered_map<LogicTypeRoot, LogicTypeInfo>
	MacroCondition::logicTypesRoot = {
		{LogicTypeRoot::NONE, {"AdvSceneSwitcher.logic.none"}},
		{LogicTypeRoot::NOT, {"AdvSceneSwitcher.logic.not"}},
};

void AdvSceneSwitcher::setupMacroTab()
{
	MacroConditionEdit *test = new MacroConditionEdit();
	ui->macroEditConditionLayout->addWidget(test);

	auto *test2 = new MacroActionEdit();
	ui->macroEditActionLayout->addWidget(test2);

	//ui->macroEditConditionHelp->hide();
	//ui->macroEditActionHelp->hide();
}
