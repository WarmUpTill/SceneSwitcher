#include "youtube-tab.hpp"
#include "youtube-connection.hpp"

#include <obs-module-helper.hpp>
#include <sync-helpers.hpp>
#include <tab-helpers.hpp>
#include <ui-helpers.hpp>

#include <QDialogButtonBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace advss {

static bool setup();
static void setupTab(QTabWidget *);
static bool setupDone = setup();

static YouTubeConnectionsTable *tabWidget = nullptr;

static bool setup()
{
	AddSetupTabCallback("youtubeConnectionTab",
			    YouTubeConnectionsTable::Create, setupTab);
	return true;
}

static void setTabVisible(QTabWidget *tab, bool visible)
{
	SetTabVisibleByName(
		tab, visible,
		obs_module_text("AdvSceneSwitcher.youtubeConnectionTab.title"));
}

static bool youtubeTabIsSelected(QTabWidget *tab)
{
	return tab->tabText(tab->currentIndex()) ==
	       obs_module_text("AdvSceneSwitcher.youtubeConnectionTab.title");
}

YouTubeConnectionsTable *YouTubeConnectionsTable::Create()
{
	tabWidget = new YouTubeConnectionsTable();
	return tabWidget;
}

void YouTubeConnectionsTable::Add()
{
	auto connection = std::make_shared<YouTubeConnection>();
	const bool accepted = YouTubeConnectionSettingsDialog::AskForSettings(
		GetSettingsWindow(), *connection);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		GetYouTubeConnections().emplace_back(connection);
	}

	YouTubeConnectionSignalManager::Instance()->Add(
		QString::fromStdString(connection->Name()));
}

void YouTubeConnectionsTable::Remove()
{
	const auto selectedRows = Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList names;
	for (const auto &row : selectedRows) {
		auto cell = Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}
		names << cell->text();
	}

	if (names.size() == 1) {
		const QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.youtubeConnectionTab.removeSingleConnectionPopup.text");
		if (!DisplayMessage(deleteWarning.arg(names.at(0)), true)) {
			return;
		}
	} else {
		const QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.youtubeConnectionTab.removeMultipleConnectionsPopup.text");
		if (!DisplayMessage(deleteWarning.arg(names.size()), true)) {
			return;
		}
	}

	for (const auto &name : names) {
		auto connection =
			GetWeakYouTubeConnectionByName(name.toStdString())
				.lock();
		if (connection) {
			connection->Revoke();
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetYouTubeConnections(), names);
	}

	for (const auto &name : names) {
		YouTubeConnectionSignalManager::Instance()->Remove(name);
	}
}

static QStringList getCellLabels(const YouTubeConnection &connection,
				 bool addName = true)
{
	QStringList result;
	if (addName) {
		result << QString::fromStdString(connection.Name());
	}
	result << obs_module_text(
		connection.IsValid()
			? "AdvSceneSwitcher.youtubeConnectionTab.yes"
			: "AdvSceneSwitcher.youtubeConnectionTab.no");
	return result;
}

static void updateConnectionStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto connection = GetWeakYouTubeConnectionByName(
					  item->text().toStdString())
					  .lock();
		if (!connection) {
			continue;
		}

		UpdateItemTableRow(table, row,
				   getCellLabels(*connection, false));
	}
}

static void openSettingsDialog()
{
	const auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	auto cell = tabWidget->Table()->item(selectedRows.last().row(), 0);
	if (!cell) {
		return;
	}

	auto connection =
		GetWeakYouTubeConnectionByName(cell->text().toStdString())
			.lock();
	if (!connection) {
		return;
	}

	YouTubeConnectionSettingsDialog::AskForSettings(GetSettingsWindow(),
							*connection);
	updateConnectionStatus(tabWidget->Table());
}

static const QStringList headers =
	QStringList()
	<< obs_module_text("AdvSceneSwitcher.youtubeConnectionTab.name.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.youtubeConnectionTab.isValid.header");

YouTubeConnectionsTable::YouTubeConnectionsTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text("AdvSceneSwitcher.youtubeConnectionTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.youtubeConnectionTab.addButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.youtubeConnectionTab.removeButton.tooltip"),
		  headers, openSettingsDialog)
{
	for (const auto &item : GetYouTubeConnections()) {
		auto connection =
			std::static_pointer_cast<YouTubeConnection>(item);
		AddItemTableRow(Table(), getCellLabels(*connection));
	}

	SetHelpVisible(GetYouTubeConnections().empty());
}

static void setupTab(QTabWidget *tab)
{
	if (GetYouTubeConnections().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(YouTubeConnectionSignalManager::Instance(),
			 &YouTubeConnectionSignalManager::Add, tab,
			 [tab](const QString &name) {
				 auto connection =
					 GetWeakYouTubeConnectionByName(
						 name.toStdString())
						 .lock();
				 if (!connection) {
					 return;
				 }
				 AddItemTableRow(tabWidget->Table(),
						 getCellLabels(*connection));
				 tabWidget->SetHelpVisible(false);
				 tabWidget->HighlightAddButton(false);
				 setTabVisible(tab, true);
			 });
	QWidget::connect(YouTubeConnectionSignalManager::Instance(),
			 &YouTubeConnectionSignalManager::Remove, tab,
			 [](const QString &name) {
				 RemoveItemTableRow(tabWidget->Table(), name);
				 if (tabWidget->Table()->rowCount() == 0) {
					 tabWidget->SetHelpVisible(true);
					 tabWidget->HighlightAddButton(true);
				 }
			 });

	QWidget::connect(tab, &QTabWidget::currentChanged, [tab]() {
		if (youtubeTabIsSelected(tab)) {
			updateConnectionStatus(tabWidget->Table());
		}
	});
}

void InvalidConnectionDialog::ShowWarning(const QString &name)
{
	auto connection =
		GetWeakYouTubeConnectionByName(name.toStdString()).lock();
	if (!connection) {
		return;
	}

	auto dialog = new InvalidConnectionDialog(name);
	dialog->setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	const bool ignore = dialog->exec() != DialogCode::Accepted;
	connection->SetWarnIfInvalid(!dialog->_doNotShowAgain->isChecked());
	dialog->deleteLater();

	if (ignore) {
		return;
	}

	YouTubeConnectionSettingsDialog::AskForSettings(GetSettingsWindow(),
							*connection);
}

InvalidConnectionDialog::InvalidConnectionDialog(const QString &name)
	: QDialog(GetSettingsWindow()),
	  _doNotShowAgain(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.youtubeConnection.warnIfInvalid.doNotShowAgain")))
{
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ignore |
					    QDialogButtonBox::Open);
	connect(buttons->button(QDialogButtonBox::Ignore),
		&QPushButton::clicked, this, &QDialog::reject);
	connect(buttons->button(QDialogButtonBox::Open), &QPushButton::clicked,
		this, &QDialog::accept);
	buttons->setCenterButtons(true);

	const QString message =
		QString(obs_module_text(
				"AdvSceneSwitcher.youtubeConnection.warnIfInvalid.message"))
			.arg(name);

	auto layout = new QVBoxLayout();
	layout->addWidget(new QLabel(message));
	layout->addWidget(_doNotShowAgain);
	layout->addWidget(buttons);
	setLayout(layout);
}

} // namespace advss
