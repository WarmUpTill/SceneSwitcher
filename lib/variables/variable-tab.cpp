#include "advanced-scene-switcher.hpp"
#include "variable.hpp"

#include <QTableWidgetItem>

namespace advss {

static void setVariableTabVisible(QTabWidget *tabWidget, bool visible)
{
	for (int idx = 0; idx < tabWidget->count(); idx++) {
		if (tabWidget->tabText(idx) !=
		    obs_module_text("AdvSceneSwitcher.variableTab.title")) {
			continue;
		}
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		// TODO: Switch to setTabVisible() once QT 5.15 is more wide spread
		tabWidget->setTabEnabled(idx, visible);
		tabWidget->setStyleSheet(
			"QTabBar::tab::disabled {width: 0; height: 0; margin: 0; padding: 0; border: none;} ");
#else
		tabWidget->setTabVisible(idx, visible);
#endif
	}
}

static QString getSaveActionString(Variable *variable)
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
				"AdvSceneSwitcher.variable.save.default")) +
			" \"" +
			QString::fromStdString(variable->GetDefaultValue()) +
			"\"";
		break;
	default:
		break;
	}
	return saveAction;
}

static QString getLastUsedString(Variable *variable)
{
	auto lastUsed = variable->SecondsSinceLastUse();
	if (!lastUsed) {
		return obs_module_text(
			"AdvSceneSwitcher.variableTab.neverNused");
	}
	QString fmt = obs_module_text("AdvSceneSwitcher.variableTab.lastUsed");
	return fmt.arg(QString::number(*lastUsed));
}

static void addVariableRow(QTableWidget *table, Variable *variable)
{
	if (!variable) {
		blog(LOG_INFO, "%s called with nullptr", __func__);
		assert(false);
		return;
	}

	int row = table->rowCount();
	table->setRowCount(row + 1);

	int col = 0;
	auto *item =
		new QTableWidgetItem(QString::fromStdString(variable->Name()));
	table->setItem(row, col, item);
	col++;
	item = new QTableWidgetItem(
		QString::fromStdString(variable->Value(false)));
	table->setItem(row, col, item);
	col++;
	item = new QTableWidgetItem(getSaveActionString(variable));
	table->setItem(row, col, item);
	col++;
	item = new QTableWidgetItem(getLastUsedString(variable));
	table->setItem(row, col, item);
	table->sortByColumn(0, Qt::AscendingOrder);
}

static void removeVariableRow(QTableWidget *table, const QString &name)
{
	for (int row = 0; row < table->rowCount(); ++row) {
		auto item = table->item(row, 0);
		if (item && item->text() == name) {
			table->removeRow(row);
			return;
		}
	}
	table->sortByColumn(0, Qt::AscendingOrder);
}

static void updateVaribleStatus(QTableWidget *table)
{
	auto lock = LockContext();
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
		item = table->item(row, 1);
		item->setText(QString::fromStdString(variable->Value(false)));
		item = table->item(row, 2);
		item->setText(getSaveActionString(variable.get()));
		item = table->item(row, 3);
		item->setText(getLastUsedString(variable.get()));
	}
}

static void renameVariable(QTableWidget *table, const QString &oldName,
			   const QString &newName)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}
		if (item->text() == oldName) {
			item->setText(newName);
			table->sortByColumn(0, Qt::AscendingOrder);
			return;
		}
	}
	blog(LOG_INFO, "%s called but entry \"%s\" not found", __func__,
	     oldName.toStdString().c_str());
	assert(false);
}

static void openSettingsDialogAtRow(QTableWidget *table, int row)
{
	auto item = table->item(row, 0);
	if (!item) {
		return;
	}
	auto weakVariable = GetWeakVariableByQString(item->text());
	auto variable = weakVariable.lock();
	if (!variable) {
		return;
	}
	auto oldName = variable->Name();

	bool accepted =
		VariableSettingsDialog::AskForSettings(table, *variable.get());
	if (accepted && oldName != variable->Name()) {
		VariableSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(variable->Name()));
	}
}

void AdvSceneSwitcher::SetupVariableTab()
{
	if (GetVariables().empty()) {
		setVariableTabVisible(ui->tabWidget, false);
	} else {
		ui->variablesHelp->hide();
	}

	static const QStringList horizontalHeaders =
		QStringList()
		<< obs_module_text("AdvSceneSwitcher.variableTab.header.name")
		<< obs_module_text("AdvSceneSwitcher.variableTab.header.value")
		<< obs_module_text(
			   "AdvSceneSwitcher.variableTab.header.saveLoadBehavior")
		<< obs_module_text(
			   "AdvSceneSwitcher.variableTab.header.lastUse");

	auto &variables = GetVariables();

	ui->variables->setColumnCount(horizontalHeaders.size());
	ui->variables->horizontalHeader()->setSectionResizeMode(
		QHeaderView::ResizeMode::Stretch);
	ui->variables->setHorizontalHeaderLabels(horizontalHeaders);

	for (const auto &var : variables) {
		auto variable = std::static_pointer_cast<Variable>(var);
		addVariableRow(ui->variables, variable.get());
	}

	ui->variables->resizeColumnsToContents();
	ui->variables->resizeRowsToContents();

	QWidget::connect(
		VariableSignalManager::Instance(),
		&VariableSignalManager::Rename,
		[this](const QString &oldName, const QString &newName) {
			renameVariable(ui->variables, oldName, newName);
		});
	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Add, this,
			 [this](const QString &name) {
				 addVariableRow(ui->variables,
						GetVariableByQString(name));
				 ui->variablesHelp->hide();
				 setVariableTabVisible(ui->tabWidget, true);
			 });
	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Remove, this,
			 [this](const QString &name) {
				 removeVariableRow(ui->variables, name);
				 if (ui->variables->rowCount() == 0) {
					 ui->variablesHelp->show();
				 }
			 });
	QWidget::connect(ui->variables, &QTableWidget::cellDoubleClicked,
			 [this](int row, int _) {
				 openSettingsDialogAtRow(ui->variables, row);
			 });

	auto timer = new QTimer(this);
	timer->setInterval(1000);
	QWidget::connect(timer, &QTimer::timeout,
			 [this]() { updateVaribleStatus(ui->variables); });
	timer->start();
}

void AdvSceneSwitcher::on_variableAdd_clicked()
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

void AdvSceneSwitcher::on_variableRemove_clicked()
{
	auto selectedItems = ui->variables->selectedItems();
	QList<int> selectedRows;
	for (auto item : selectedItems) {
		int row = item->row();
		if (!selectedRows.contains(row)) {
			selectedRows.append(row);
		}
	}

	if (selectedRows.empty()) {
		return;
	}

	QStringList names;
	for (int row : selectedRows) {
		auto item = ui->variables->item(row, 0);
		if (!item) {
			continue;
		}
		names << item->text();
	}

	for (const auto &name : names) {
		VariableSignalManager::Instance()->Remove(name);
	}

	auto lock = LockContext();
	for (const auto &name : names) {
		auto variable = GetVariableByQString(name);
		if (!variable) {
			continue;
		}

		auto &variables = GetVariables();
		variables.erase(
			std::remove_if(
				variables.begin(), variables.end(),
				[variable](const std::shared_ptr<Item> &item) {
					return item.get() == variable;
				}),
			variables.end());
	}
}

} // namespace advss
