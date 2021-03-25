#include "headers/macro-entry.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/advanced-scene-switcher.hpp"

void AdvSceneSwitcher::setupMacroTab()
{
	MacroConditionEdit *test = new MacroConditionEdit();
	ui->macroEditConditionLayout->addWidget(test);

	auto *test2 = new MacroActionEdit();
	ui->macroEditActionLayout->addWidget(test2);

	//ui->macroEditConditionHelp->hide();
	//ui->macroEditActionHelp->hide();
}