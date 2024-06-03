#include "variable-tab.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"
#include "variable.hpp"

#include <QTimer>

namespace advss {

static bool registerTab();
static void setupTab(QTabWidget *);
static bool registerTabDone = registerTab();

static VariableTable *tabWidget = nullptr;

static bool registerTab()
{
	AddPluginInitStep([]() {
		AddSetupTabCallback("variableTab", VariableTable::Create,
				    setupTab);
	});
	return true;
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text("AdvSceneSwitcher.variableTab.title"));
}

VariableTable *VariableTable::Create()
{
	tabWidget = new VariableTable();
	return tabWidget;
}

void VariableTable::Add()
{
	auto newVariable = std::make_shared<Variable>();
	auto accepted =
		VariableSettingsDialog::AskForSettings(this, *newVariable);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		auto &variables = GetVariables();
		variables.emplace_back(newVariable);
	}

	VariableSignalManager::Instance()->Add(
		QString::fromStdString(newVariable->Name()));
}

static QString formatSaveActionText(Variable *variable)
{
	QString saveAction;

	switch (variable->GetSaveAction()) {
	case Variable::SaveAction::DONT_SAVE:
		saveAction = obs_module_text(
			"AdvSceneSwitcher.variable.save.dontSave");
		break;
	case Variable::SaveAction::SAVE:
		saveAction =
			obs_module_text("AdvSceneSwitcher.variable.save.save");
		break;
	case Variable::SaveAction::SET_DEFAULT:
		saveAction =
			QString(obs_module_text(
					"AdvSceneSwitcher.variableTab.saveLoadBehavior.text.default"))
				.arg(QString::fromStdString(
					variable->GetDefaultValue()));
		break;
	default:
		break;
	}

	return saveAction;
}

static QString formatLastUsedText(Variable *variable)
{
	auto lastUsed = variable->GetSecondsSinceLastUse();
	if (!lastUsed) {
		return obs_module_text(
			"AdvSceneSwitcher.variableTab.lastUsed.text.never");
	}

	QString text =
		obs_module_text("AdvSceneSwitcher.variableTab.lastUsed.text");
	return text.arg(QString::number(*lastUsed));
}

static QString formatLastChangedText(Variable *variable)
{
	auto lastChanged = variable->GetSecondsSinceLastChange();
	if (!lastChanged) {
		return obs_module_text(
			"AdvSceneSwitcher.variableTab.lastChanged.text.none");
	}

	QString text = obs_module_text(
		"AdvSceneSwitcher.variableTab.lastChanged.text");
	return text.arg(QString::number(*lastChanged));
}

static QString formatLastChangedTooltip(Variable *variable)
{
	auto lastChanged = variable->GetSecondsSinceLastChange();
	if (!lastChanged) {
		return QString();
	}

	QString tooltip = obs_module_text(
		"AdvSceneSwitcher.variableTab.lastChanged.tooltip");
	return tooltip.arg(QString::number(variable->GetValueChangeCount()))
		.arg(QString::fromStdString(variable->GetPreviousValue()));
}

static QStringList getCellLabels(Variable *variable, bool addName = true)
{
	assert(variable);

	auto result = QStringList();
	if (addName) {
		result << QString::fromStdString(variable->Name());
	}
	result << QString::fromStdString(variable->Value(false))
	       << formatSaveActionText(variable) << formatLastUsedText(variable)
	       << formatLastChangedText(variable);
	return result;
}

static void updateVariableStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto weakVariable = GetWeakVariableByQString(item->text());
		auto variable = weakVariable.lock();
		if (!variable) {
			continue;
		}

		UpdateItemTableRow(table, row,
				   getCellLabels(variable.get(), false));

		// Special tooltip handling for the "last used" cell
		const auto lastUsedItem = table->item(row, 4);
		if (!lastUsedItem) {
			continue;
		}
		const auto lastUsedTooltip =
			formatLastChangedTooltip(variable.get());
		lastUsedItem->setToolTip(lastUsedTooltip);
	}
}

static void openSettingsDialog()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	auto cell = tabWidget->Table()->item(selectedRows.last().row(), 0);
	if (!cell) {
		return;
	}

	auto weakVariable = GetWeakVariableByQString(cell->text());
	auto variable = weakVariable.lock();
	if (!variable) {
		return;
	}

	auto oldName = variable->Name();
	bool accepted = VariableSettingsDialog::AskForSettings(
		tabWidget->Table(), *variable.get());
	if (accepted && oldName != variable->Name()) {
		VariableSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(variable->Name()));
	}
}

void VariableTable::Remove()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList varNames;
	for (const auto &row : selectedRows) {
		auto cell = tabWidget->Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}

		varNames << cell->text();
	}

	int varNameCount = varNames.size();
	if (varNameCount == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.variableTab.removeSingleVariablePopup.text");
		if (!DisplayMessage(deleteWarning.arg(varNames.at(0)), true)) {
			return;
		}
	} else {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.variableTab.removeMultipleVariablesPopup.text");
		if (!DisplayMessage(deleteWarning.arg(varNameCount), true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetVariables(), varNames);
	}

	for (const auto &name : varNames) {
		VariableSignalManager::Instance()->Remove(name);
	}
}

VariableTable::VariableTable(QTabWidget *parent)
	: ResourceTable(
		  parent, obs_module_text("AdvSceneSwitcher.variableTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.variableTab.variableAddButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.variableTab.variableRemoveButton.tooltip"),
		  QStringList()
			  << obs_module_text(
				     "AdvSceneSwitcher.variableTab.name.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.variableTab.value.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.variableTab.saveLoadBehavior.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.variableTab.lastUsed.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.variableTab.lastChanged.header"),
		  openSettingsDialog)
{
	for (const auto &variable : GetVariables()) {
		auto v = std::static_pointer_cast<Variable>(variable);
		AddItemTableRow(Table(), getCellLabels(v.get()));
	}

	SetHelpVisible(GetVariables().empty());
}

static void setupTab(QTabWidget *tab)
{
	if (GetVariables().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Rename, tab,
			 [](const QString &oldName, const QString &newName) {
				 RenameItemTableRow(tabWidget->Table(), oldName,
						    newName);
			 });
	QWidget::connect(
		VariableSignalManager::Instance(), &VariableSignalManager::Add,
		tab, [tab](const QString &name) {
			AddItemTableRow(
				tabWidget->Table(),
				getCellLabels(GetVariableByQString(name)));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Remove, tab,
			 [](const QString &name) {
				 RemoveItemTableRow(tabWidget->Table(), name);
				 if (tabWidget->Table()->rowCount() == 0) {
					 tabWidget->SetHelpVisible(true);
					 tabWidget->HighlightAddButton(true);
				 }
			 });

	auto timer = new QTimer(tabWidget);
	timer->setInterval(1000);
	QWidget::connect(timer, &QTimer::timeout,
			 []() { updateVariableStatus(tabWidget->Table()); });
	timer->start();
}

} // namespace advss
