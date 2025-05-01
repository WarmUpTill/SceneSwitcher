#include "mqtt-tab.hpp"
#include "mqtt-helpers.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "ui-helpers.hpp"

#include <QTabWidget>
#include <QTimer>

namespace advss {

static bool registerTab();
static void setupTab(QTabWidget *);
static bool registerTabDone = registerTab();

static MqttConnectionsTable *tabWidget = nullptr;

static bool registerTab()
{
	AddSetupTabCallback("mqttConnectionTab", MqttConnectionsTable::Create,
			    setupTab);
	return true;
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text("AdvSceneSwitcher.mqttConnectionTab.title"));
}

MqttConnectionsTable *MqttConnectionsTable::Create()
{
	tabWidget = new MqttConnectionsTable();
	return tabWidget;
}

void MqttConnectionsTable::Add()
{
	auto newConnection = std::make_shared<MqttConnection>();
	auto accepted = MqttConnectionSettingsDialog::AskForSettings(
		GetSettingsWindow(), *newConnection);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		auto &connections = GetMqttConnections();
		connections.emplace_back(newConnection);
	}

	MqttConnectionSignalManager::Instance()->Add(
		QString::fromStdString(newConnection->Name()));
}

void MqttConnectionsTable::Remove()
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
			"AdvSceneSwitcher.mqttConnectionTab.removeSingleConnectionPopup.text");
		if (!DisplayMessage(deleteWarning.arg(connectionNames.at(0)),
				    true)) {
			return;
		}
	} else {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.mqttConnectionTab.removeMultipleConnectionsPopup.text");
		if (!DisplayMessage(deleteWarning.arg(connectionNameCount),
				    true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetMqttConnections(), connectionNames);
	}

	for (const auto &name : connectionNames) {
		MqttConnectionSignalManager::Instance()->Remove(name);
	}
}

static QStringList getCellLabels(MqttConnection *connection,
				 bool addName = true)
{
	assert(connection);

	auto result = QStringList();
	if (addName) {
		result << QString::fromStdString(connection->Name());
	}
	result << connection->GetURI()
	       << QString::number(connection->GetTopicSubscriptionCount())
	       << connection->GetStatus();
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

	auto weakConnection = GetWeakMqttConnectionByQString(cell->text());
	auto connection = weakConnection.lock();
	if (!connection) {
		return;
	}

	auto oldName = connection->Name();
	bool accepted = MqttConnectionSettingsDialog::AskForSettings(
		GetSettingsWindow(), *connection.get());
	if (accepted && oldName != connection->Name()) {
		MqttConnectionSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(connection->Name()));
	}
}

static const QStringList headers =
	QStringList()
	<< obs_module_text("AdvSceneSwitcher.mqttConnectionTab.name.header")
	<< obs_module_text("AdvSceneSwitcher.mqttConnectionTab.address.header")
	<< obs_module_text("AdvSceneSwitcher.mqttConnectionTab.topics.header")
	<< obs_module_text("AdvSceneSwitcher.mqttConnectionTab.status.header");

MqttConnectionsTable::MqttConnectionsTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text("AdvSceneSwitcher.mqttConnectionTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.mqttConnectionTab.websocketConnectionAddButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.mqttConnectionTab.websocketConnectionRemoveButton.tooltip"),
		  headers, openSettingsDialog)
{
	for (const auto &connection : GetMqttConnections()) {
		auto c = std::static_pointer_cast<MqttConnection>(connection);
		AddItemTableRow(Table(), getCellLabels(c.get()));
	}

	SetHelpVisible(GetMqttConnections().empty());
}

static void updateConnectionStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto weakConnection =
			GetWeakMqttConnectionByQString(item->text());
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
	if (GetMqttConnections().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(MqttConnectionSignalManager::Instance(),
			 &MqttConnectionSignalManager::Rename, tab,
			 [](const QString &oldName, const QString &newName) {
				 RenameItemTableRow(tabWidget->Table(), oldName,
						    newName);
			 });
	QWidget::connect(
		MqttConnectionSignalManager::Instance(),
		&MqttConnectionSignalManager::Add, tab,
		[tab](const QString &name) {
			AddItemTableRow(
				tabWidget->Table(),
				getCellLabels(GetMqttConnectionByName(name)));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(MqttConnectionSignalManager::Instance(),
			 &MqttConnectionSignalManager::Remove, tab,
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
