#include "twitch-tab.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "token.hpp"
#include "ui-helpers.hpp"

#include <QDialogButtonBox>
#include <QTabWidget>

namespace advss {

static bool setup();
static void setupTab(QTabWidget *);
static bool setupDone = setup();

static TwitchConnectionsTable *tabWidget = nullptr;

static QStringList getInvalidTokens();

static bool setup()
{
	AddSetupTabCallback("twitchConnectionTab",
			    TwitchConnectionsTable::Create, setupTab);

	static const auto showInvalidWarnings = []() {
		const auto invalidTokens = getInvalidTokens();
		for (const auto &token : invalidTokens) {
			QueueUITask(
				[](void *tokenPtr) {
					auto tokenName = static_cast<QString *>(
						tokenPtr);
					InvalidTokenDialog::ShowWarning(
						*tokenName);
				},
				(void *)&token);
		}
	};

	AddLoadStep([](obs_data_t *) {
		AddPostLoadStep([]() { showInvalidWarnings(); });
	});

	return true;
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text("AdvSceneSwitcher.twitchConnectionTab.title"));
}

static bool twitchTabIsSelected(QTabWidget *tabWidget)
{
	return tabWidget->tabText(tabWidget->currentIndex()) ==
	       obs_module_text("AdvSceneSwitcher.twitchConnectionTab.title");
}

TwitchConnectionsTable *TwitchConnectionsTable::Create()
{
	tabWidget = new TwitchConnectionsTable();
	return tabWidget;
}

void TwitchConnectionsTable::Add()
{
	auto newToken = std::make_shared<TwitchToken>();
	auto accepted = TwitchTokenSettingsDialog::AskForSettings(
		GetSettingsWindow(), *newToken);
	if (!accepted) {
		return;
	}

	{
		auto lock = LockContext();
		auto &tokens = GetTwitchTokens();
		tokens.emplace_back(newToken);
	}

	TwitchConnectionSignalManager::Instance()->Add(
		QString::fromStdString(newToken->Name()));
}

void TwitchConnectionsTable::Remove()
{
	auto selectedRows =
		tabWidget->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList tokenNames;
	for (auto row : selectedRows) {
		auto cell = tabWidget->Table()->item(row.row(), 0);
		if (!cell) {
			continue;
		}

		tokenNames << cell->text();
	}

	int tokenNameCount = tokenNames.size();
	if (tokenNameCount == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.twitchConnectionTab.removeSingleConnectionPopup.text");
		if (!DisplayMessage(deleteWarning.arg(tokenNames.at(0)),
				    true)) {
			return;
		}
	} else {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.twitchConnectionTab.removeMultipleConnectionsPopup.text");
		if (!DisplayMessage(deleteWarning.arg(tokenNameCount), true)) {
			return;
		}
	}

	{
		auto lock = LockContext();
		RemoveItemsByName(GetTwitchTokens(), tokenNames);
	}

	for (const auto &name : tokenNames) {
		TwitchConnectionSignalManager::Instance()->Remove(name);
	}
}

static QStringList getCellLabels(TwitchToken *token, bool addName = true)
{
	assert(token);

	auto result = QStringList();
	if (addName) {
		result << QString::fromStdString(token->Name());
	}
	result << QString::fromStdString(obs_module_text(
			  token->IsValid()
				  ? "AdvSceneSwitcher.twitchConnectionTab.yes"
				  : "AdvSceneSwitcher.twitchConnectionTab.no"))
	       << QString::number(token->PermissionCount());
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

	auto weakToken = GetWeakTwitchTokenByQString(cell->text());
	auto token = weakToken.lock();
	if (!token) {
		return;
	}

	TwitchTokenSettingsDialog::AskForSettings(GetSettingsWindow(),
						  *token.get());
}

static const QStringList headers =
	QStringList()
	<< obs_module_text("AdvSceneSwitcher.twitchConnectionTab.name.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.twitchConnectionTab.isValid.header")
	<< obs_module_text(
		   "AdvSceneSwitcher.twitchConnectionTab.permissionCount.header");

TwitchConnectionsTable::TwitchConnectionsTable(QTabWidget *parent)
	: ResourceTable(
		  parent,
		  obs_module_text("AdvSceneSwitcher.twitchConnectionTab.help"),
		  obs_module_text(
			  "AdvSceneSwitcher.twitchConnectionTab.twitchConnectionAddButton.tooltip"),
		  obs_module_text(
			  "AdvSceneSwitcher.twitchConnectionTab.twitchConnectionRemoveButton.tooltip"),
		  headers, openSettingsDialog)
{
	for (const auto &token : GetTwitchTokens()) {
		auto t = std::static_pointer_cast<TwitchToken>(token);
		AddItemTableRow(Table(), getCellLabels(t.get()));
	}

	SetHelpVisible(GetTwitchTokens().empty());
}

static void updateConnectionStatus(QTableWidget *table)
{
	for (int row = 0; row < table->rowCount(); row++) {
		auto item = table->item(row, 0);
		if (!item) {
			continue;
		}

		auto weakToken = GetWeakTwitchTokenByQString(item->text());
		auto token = weakToken.lock();
		if (!token) {
			continue;
		}

		UpdateItemTableRow(table, row,
				   getCellLabels(token.get(), false));
	}
}

static QStringList getInvalidTokens()
{
	QStringList tokens;
	for (const auto &t : GetTwitchTokens()) {
		if (!t) {
			continue;
		}

		auto token = std::static_pointer_cast<TwitchToken>(t);
		if (!token->WarnIfInvalid() || token->IsValid(true)) {
			continue;
		}

		tokens << QString::fromStdString(token->GetName());
	}

	return tokens;
}

static void setupTab(QTabWidget *tab)
{
	if (GetTwitchTokens().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(
		TwitchConnectionSignalManager::Instance(),
		&TwitchConnectionSignalManager::Add, tab,
		[tab](const QString &name) {
			AddItemTableRow(
				tabWidget->Table(),
				getCellLabels(GetWeakTwitchTokenByQString(name)
						      .lock()
						      .get()));
			tabWidget->SetHelpVisible(false);
			tabWidget->HighlightAddButton(false);
			setTabVisible(tab, true);
		});
	QWidget::connect(TwitchConnectionSignalManager::Instance(),
			 &TwitchConnectionSignalManager::Remove, tab,
			 [](const QString &name) {
				 RemoveItemTableRow(tabWidget->Table(), name);
				 if (tabWidget->Table()->rowCount() == 0) {
					 tabWidget->SetHelpVisible(true);
					 tabWidget->HighlightAddButton(true);
				 }
			 });

	QWidget::connect(tab, &QTabWidget::currentChanged, [tab]() {
		if (twitchTabIsSelected(tab)) {
			updateConnectionStatus(tabWidget->Table());
		}
	});

	const auto invalidTokens = getInvalidTokens();
	for (const auto &token : invalidTokens) {
		// Constructing the warning dialog in the constructor of the settings
		// window might lead to a crash, so wait for the settings window
		// constructor to complete
		QTimer::singleShot(0, tab, [token]() {
			InvalidTokenDialog::ShowWarning(token);
		});
	}
}

void InvalidTokenDialog::ShowWarning(const QString &tokenName)
{
	auto weakToken = GetWeakTwitchTokenByQString(tokenName);
	auto token = weakToken.lock();
	if (!token) {
		return;
	}

	auto dialog = new InvalidTokenDialog(tokenName);
	dialog->setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	const bool ignore = dialog->exec() != DialogCode::Accepted;
	token->SetWarnIfInvalid(!dialog->_doNotShowAgain->isChecked());
	dialog->deleteLater();

	if (ignore) {
		return;
	}

	TwitchTokenSettingsDialog::AskForSettings(GetSettingsWindow(),
						  *token.get());
}

InvalidTokenDialog::InvalidTokenDialog(const QString &name)
	: QDialog(GetSettingsWindow()),
	  _doNotShowAgain(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.twitchToken.warnIfInvalid.doNotShowAgain")))
{
	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ignore |
					    QDialogButtonBox::Open);

	connect(buttons->button(QDialogButtonBox::Ignore),
		&QPushButton::clicked, this, &QDialog::reject);
	connect(buttons->button(QDialogButtonBox::Open), &QPushButton::clicked,
		this, &QDialog::accept);
	buttons->setCenterButtons(true);

	const QString format(obs_module_text(
		"AdvSceneSwitcher.twitchToken.warnIfInvalid.message"));
	const QString message = format.arg(name);

	auto layout = new QVBoxLayout;
	layout->addWidget(new QLabel(message));
	layout->addWidget(_doNotShowAgain);
	layout->addWidget(buttons);
	setLayout(layout);
}

} // namespace advss
