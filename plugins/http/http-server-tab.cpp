#include "http-server-tab.hpp"
#include "http-server.hpp"
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

static HttpServersTable *tabWidget = nullptr;

static bool registerTab()
{
	AddSetupTabCallback("httpServerTab", HttpServersTable::Create,
			    setupTab);
	return true;
}

static void setTabVisible(QTabWidget *tab, bool visible)
{
	SetTabVisibleByName(
		tab, visible,
		obs_module_text("AdvSceneSwitcher.httpServerTab.title"));
}

HttpServersTable *HttpServersTable::Create()
{
	tabWidget = new HttpServersTable();
	return tabWidget;
}

void HttpServersTable::Add()
{
	auto newServer = std::make_shared<HttpServer>();
	bool accepted = HttpServerSettingsDialog::AskForSettings(
		GetSettingsWindow(), *newServer);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		GetHttpServers().emplace_back(newServer);
	}

	HttpServerSignalManager::Instance()->Add(
		QString::fromStdString(newServer->Name()));
}

void HttpServersTable::Remove()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList serverNames;
	for (const auto &row : selectedRows) {
		auto cell = tabWidget->Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}
		serverNames << cell->text();
	}

	const int count = serverNames.size();
	if (count == 1) {
		const QString msg = obs_module_text(
			"AdvSceneSwitcher.httpServerTab.removeSingle.text");
		if (!DisplayMessage(msg.arg(serverNames.at(0)), true)) {
			return;
		}
	} else {
		const QString msg = obs_module_text(
			"AdvSceneSwitcher.httpServerTab.removeMultiple.text");
		if (!DisplayMessage(msg.arg(count), true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetHttpServers(), serverNames);
	}

	for (const auto &name : serverNames) {
		HttpServerSignalManager::Instance()->Remove(name);
	}
}

static QStringList getCellLabels(HttpServer *server, bool addName = true)
{
	assert(server);
	QStringList result;
	if (addName) {
		result << QString::fromStdString(server->Name());
	}
	result << QString::number(server->GetPort())
	       << obs_module_text(
			  server->IsListening()
				  ? "AdvSceneSwitcher.httpServer.status.listening"
				  : "AdvSceneSwitcher.httpServer.status.stopped");
	return result;
}

static void updateServerStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); ++row) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}
		auto weakServer = GetWeakHttpServerByQString(item->text());
		auto server = weakServer.lock();
		if (!server) {
			continue;
		}
		UpdateItemTableRow(table, row,
				   getCellLabels(server.get(), false));
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

	auto weakServer = GetWeakHttpServerByQString(cell->text());
	auto server = weakServer.lock();
	if (!server) {
		return;
	}

	const auto oldName = server->Name();
	bool accepted = HttpServerSettingsDialog::AskForSettings(
		GetSettingsWindow(), *server);
	if (accepted && oldName != server->Name()) {
		HttpServerSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(server->Name()));
	}
	updateServerStatus(tabWidget->Table());
}

static const QStringList headers =
	QStringList()
	<< obs_module_text("AdvSceneSwitcher.httpServerTab.name.header")
	<< obs_module_text("AdvSceneSwitcher.httpServerTab.port.header")
	<< obs_module_text("AdvSceneSwitcher.httpServerTab.status.header");

HttpServersTable::HttpServersTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text("AdvSceneSwitcher.httpServerTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.httpServerTab.addButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.httpServerTab.removeButton.tooltip"),
		  headers, openSettingsDialog)
{
	for (const auto &s : GetHttpServers()) {
		auto server = std::static_pointer_cast<HttpServer>(s);
		AddItemTableRow(Table(), getCellLabels(server.get()));
	}
	SetHelpVisible(GetHttpServers().empty());
}

static void setupTab(QTabWidget *tab)
{
	if (GetHttpServers().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(HttpServerSignalManager::Instance(),
			 &HttpServerSignalManager::Rename, tab,
			 [](const QString &oldName, const QString &newName) {
				 RenameItemTableRow(tabWidget->Table(), oldName,
						    newName);
			 });
	QWidget::connect(
		HttpServerSignalManager::Instance(),
		&HttpServerSignalManager::Add, tab, [tab](const QString &name) {
			AddItemTableRow(tabWidget->Table(),
					getCellLabels(GetHttpServerByName(
						name.toStdString())));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(HttpServerSignalManager::Instance(),
			 &HttpServerSignalManager::Remove, tab,
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
			 []() { updateServerStatus(tabWidget->Table()); });
	timer->start();
}

} // namespace advss
