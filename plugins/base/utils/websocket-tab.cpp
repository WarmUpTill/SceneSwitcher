#include "websocket-tab.hpp"
#include "connection-manager.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"

#include <QTabWidget>

namespace advss {

static bool registerTab();
static void setupTab(QTabWidget *);
static bool registerTabDone = registerTab();

static WSConnectionsTable *tabWidget = nullptr;

static bool registerTab()
{
	AddSetupTabCallback("websocketConnectionTab",
			    WSConnectionsTable::Create, setupTab);
	return true;
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text(
			"AdvSceneSwitcher.websocketConnectionTab.title"));
}

WSConnectionsTable *WSConnectionsTable::Create()
{
	tabWidget = new WSConnectionsTable();
	return tabWidget;
}

void WSConnectionsTable::Add()
{
	auto newConnection = std::make_shared<WSConnection>();
	auto accepted = WSConnectionSettingsDialog::AskForSettings(
		this, *newConnection);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		auto &connections = GetConnections();
		connections.emplace_back(newConnection);
	}

	ConnectionSelectionSignalManager::Instance()->Add(
		QString::fromStdString(newConnection->Name()));
}

void WSConnectionsTable::Remove()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList connectionNames;
	for (auto row : selectedRows) {
		auto cell = tabWidget->Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}

		connectionNames << cell->text();
	}

	int connectionNameCount = connectionNames.size();
	if (connectionNameCount == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.websocketConnectionTab.removeSingleConnectionPopup.text");
		if (!DisplayMessage(deleteWarning.arg(connectionNames.at(0)),
				    true)) {
			return;
		}
	} else {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.websocketConnectionTab.removeMultipleConnectionsPopup.text");
		if (!DisplayMessage(deleteWarning.arg(connectionNameCount),
				    true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetConnections(), connectionNames);
	}

	for (const auto &name : connectionNames) {
		ConnectionSelectionSignalManager::Instance()->Remove(name);
	}
}

static QString formatProtocolUsageText(WSConnection *connection)
{
	return obs_module_text(
		connection->IsUsingOBSProtocol()
			? "AdvSceneSwitcher.websocketConnectionTab.protocol.yes"
			: "AdvSceneSwitcher.websocketConnectionTab.protocol.no");
}

static QStringList getCellLabels(WSConnection *connection, bool addName = true)
{
	assert(connection);

	auto result = QStringList();
	if (addName) {
		result << QString::fromStdString(connection->Name());
	}
	result << QString::fromStdString(connection->GetURI())
	       << QString::number(connection->GetPort())
	       << formatProtocolUsageText(connection);
	return result;
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

	auto weakConnection = GetWeakConnectionByQString(cell->text());
	auto connection = weakConnection.lock();
	if (!connection) {
		return;
	}

	auto oldName = connection->Name();
	bool accepted = WSConnectionSettingsDialog::AskForSettings(
		tabWidget->Table(), *connection.get());
	if (accepted && oldName != connection->Name()) {
		ConnectionSelectionSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(connection->Name()));
	}
}

static const QStringList headers =
	QStringList()
	<< obs_module_text(
		   "AdvSceneSwitcher.websocketConnectionTab.name.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.websocketConnectionTab.address.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.websocketConnectionTab.port.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.websocketConnectionTab.protocol.header");

WSConnectionsTable::WSConnectionsTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text(
			  "AdvSceneSwitcher.websocketConnectionTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.websocketConnectionTab.websocketConnectionAddButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.websocketConnectionTab.websocketConnectionRemoveButton.tooltip"),
		  headers, openSettingsDialog)
{
	for (const auto &connection : GetConnections()) {
		auto c = std::static_pointer_cast<WSConnection>(connection);
		AddItemTableRow(Table(), getCellLabels(c.get()));
	}

	SetHelpVisible(GetConnections().empty());
}

static void updateConnectionStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto weakConnection = GetWeakConnectionByQString(item->text());
		auto connection = weakConnection.lock();
		if (!connection) {
			continue;
		}

		UpdateItemTableRow(table, row,
				   getCellLabels(connection.get(), false));
	}
}

static void setupTab(QTabWidget *tab)
{
	if (GetConnections().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(ConnectionSelectionSignalManager::Instance(),
			 &ConnectionSelectionSignalManager::Rename, tab,
			 [](const QString &oldName, const QString &newName) {
				 RenameItemTableRow(tabWidget->Table(), oldName,
						    newName);
			 });
	QWidget::connect(
		ConnectionSelectionSignalManager::Instance(),
		&ConnectionSelectionSignalManager::Add, tab,
		[tab](const QString &name) {
			AddItemTableRow(
				tabWidget->Table(),
				getCellLabels(GetConnectionByName(name)));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(ConnectionSelectionSignalManager::Instance(),
			 &ConnectionSelectionSignalManager::Remove, tab,
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
			 []() { updateConnectionStatus(tabWidget->Table()); });
	timer->start();
}

} // namespace advss
