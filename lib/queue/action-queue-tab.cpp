#include "action-queue-tab.hpp"
#include "action-queue.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"

#include <QTimer>

namespace advss {

static void setupTab(QTabWidget *);

static ActionQueueTable *tabWidget = nullptr;

void RegisterActionQueueTab()
{
	AddPluginInitStep([]() {
		AddSetupTabCallback("actionQueueTab", ActionQueueTable::Create,
				    setupTab);
	});
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text("AdvSceneSwitcher.actionQueueTab.title"));
}

ActionQueueTable *ActionQueueTable::Create()
{
	tabWidget = new ActionQueueTable();
	return tabWidget;
}

void ActionQueueTable::Add()
{
	auto newQueue = std::make_shared<ActionQueue>();
	auto accepted =
		ActionQueueSettingsDialog::AskForSettings(this, *newQueue);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		auto &queues = GetActionQueues();
		queues.emplace_back(newQueue);
	}

	ActionQueueSignalManager::Instance()->Add(
		QString::fromStdString(newQueue->Name()));
}

static QStringList getCellLabels(ActionQueue *queue, bool addName = true)
{
	assert(queue);

	auto result = QStringList();
	if (addName) {
		result << QString::fromStdString(queue->Name());
	}
	result << QString::number(queue->Size())
	       << QString::fromStdString(obs_module_text(
			  queue->IsRunning()
				  ? "AdvSceneSwitcher.actionQueueTab.yes"
				  : "AdvSceneSwitcher.actionQueueTab.no"))
	       << QString::fromStdString(obs_module_text(
			  queue->RunsOnStartup()
				  ? "AdvSceneSwitcher.actionQueueTab.yes"
				  : "AdvSceneSwitcher.actionQueueTab.no"));
	return result;
}

static void updateQueueStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto weakQueue = GetWeakActionQueueByQString(item->text());
		auto queue = weakQueue.lock();
		if (!queue) {
			continue;
		}

		UpdateItemTableRow(table, row,
				   getCellLabels(queue.get(), false));
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

	auto weakQueue = GetWeakActionQueueByQString(cell->text());
	auto queue = weakQueue.lock();
	if (!queue) {
		return;
	}

	auto oldName = queue->Name();
	bool accepted = ActionQueueSettingsDialog::AskForSettings(
		tabWidget->Table(), *queue.get());
	if (accepted && oldName != queue->Name()) {
		ActionQueueSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(queue->Name()));
	}
}

static void removeQueuesWithNames(const QStringList &queueNames)
{
	for (const auto &name : queueNames) {
		auto queue = GetWeakActionQueueByQString(name).lock();
		if (!queue) {
			continue;
		}

		auto &queues = GetActionQueues();
		queues.erase(
			std::remove_if(
				queues.begin(), queues.end(),
				[queue](const std::shared_ptr<Item> &item) {
					return item == queue;
				}),
			queues.end());
	}
}

void ActionQueueTable::Remove()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList queueNames;
	for (const auto &row : selectedRows) {
		auto cell = tabWidget->Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}

		queueNames << cell->text();
	}

	int queueNameCount = queueNames.size();
	if (queueNameCount == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.actionQueueTab.removeSingleQueuePopup.text");
		if (!DisplayMessage(deleteWarning.arg(queueNames.at(0)),
				    true)) {
			return;
		}
	} else {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.actionQueueTab.removeMultipleQueuesPopup.text");
		if (!DisplayMessage(deleteWarning.arg(queueNameCount), true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetActionQueues(), queueNames);
	}

	for (const auto &name : queueNames) {
		ActionQueueSignalManager::Instance()->Remove(name);
	}
}

ActionQueueTable::ActionQueueTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text("AdvSceneSwitcher.actionQueueTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.actionQueueTab.queueAddButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.actionQueueTab.queueRemoveButton.tooltip"),
		  QStringList()
			  << obs_module_text(
				     "AdvSceneSwitcher.actionQueueTab.name.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.actionQueueTab.size.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.actionQueueTab.isRunning.header")
			  << obs_module_text(
				     "AdvSceneSwitcher.actionQueueTab.runOnStartup.header"),
		  openSettingsDialog)
{
	for (const auto &queue : GetActionQueues()) {
		auto q = std::static_pointer_cast<ActionQueue>(queue);
		AddItemTableRow(Table(), getCellLabels(q.get()));
	}

	SetHelpVisible(GetActionQueues().empty());
}

static void setupTab(QTabWidget *tab)
{
	if (GetActionQueues().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(ActionQueueSignalManager::Instance(),
			 &ActionQueueSignalManager::Rename,
			 [](const QString &oldName, const QString &newName) {
				 RenameItemTableRow(tabWidget->Table(), oldName,
						    newName);
			 });
	QWidget::connect(
		ActionQueueSignalManager::Instance(),
		&ActionQueueSignalManager::Add, [tab](const QString &name) {
			AddItemTableRow(
				tabWidget->Table(),
				getCellLabels(GetWeakActionQueueByQString(name)
						      .lock()
						      .get()));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(ActionQueueSignalManager::Instance(),
			 &ActionQueueSignalManager::Remove,
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
			 []() { updateQueueStatus(tabWidget->Table()); });
	timer->start();
}

} // namespace advss
