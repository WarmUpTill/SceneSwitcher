#include "variable-tab.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"
#include "sync-helpers.hpp"
#include "tab-helpers.hpp"
#include "time-helpers.hpp"
#include "ui-helpers.hpp"
#include "variable.hpp"

#include <obs-frontend-api.h>

#include <QLayout>
#include <QTimer>

namespace advss {

static bool registerTab();
static void setupTab(QTabWidget *);
static bool registerTabDone = registerTab();

static VariableTable *dockWidget = nullptr;
static bool addDock = false;

static VariableTable::Settings tabSettings;
static VariableTable::Settings dockSettings;

static void save(obs_data_t *data)
{
	tabSettings.Save(data, "tabSettings");
	dockSettings.Save(data, "dockSettings");
	obs_data_set_bool(data, "addVariablesDock", addDock);
}

static void enableDock(bool enable)
{
	if (OBSIsShuttingDown()) {
		return;
	}

	obs_frontend_remove_dock("advss-variable-dock");

	addDock = enable;
	if (!addDock) {
		return;
	}

	dockWidget = new VariableTable(dockSettings);
	dockWidget->HideDockOptions();
	if (obs_frontend_add_dock_by_id(
		    "advss-variable-dock",
		    obs_module_text("AdvSceneSwitcher.variableTab.title"),
		    dockWidget)) {
		return;
	}

	blog(LOG_INFO, "failed to register variable dock!");
	dockWidget->deleteLater();
	dockWidget = nullptr;
}

static void load(obs_data_t *data)
{
	tabSettings.Load(data, "tabSettings");
	dockSettings.Load(data, "dockSettings");

	enableDock(obs_data_get_bool(data, "addVariablesDock"));
}

static bool registerTab()
{
	AddPluginInitStep([]() {
		AddSetupTabCallback("variableTab",
				    VariableTable::CreateTabTable, setupTab);
	});
	AddSaveStep(save);
	AddLoadStep(load);
	return true;
}

static void setTabVisible(QTabWidget *tabWidget, bool visible)
{
	SetTabVisibleByName(
		tabWidget, visible,
		obs_module_text("AdvSceneSwitcher.variableTab.title"));
}

VariableTable *VariableTable::CreateTabTable()
{
	return new VariableTable(tabSettings);
}

void VariableTable::HideDockOptions() const
{
	_addDock->hide();
}

void VariableTable::Add()
{
	auto newVariable = std::make_shared<Variable>();
	auto accepted = VariableSettingsDialog::AskForSettings(
		GetSettingsWindow(), *newVariable);
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

	return FormatRelativeTime(*lastUsed);
}

static QString formatLastChangedText(Variable *variable)
{
	auto lastChanged = variable->GetSecondsSinceLastChange();
	if (!lastChanged) {
		return obs_module_text(
			"AdvSceneSwitcher.variableTab.lastChanged.text.none");
	}

	return FormatRelativeTime(*lastChanged);
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

static void openSettingsDialog(VariableTable *table)
{
	auto selectedRows = table->Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	auto cell = table->Table()->item(selectedRows.last().row(), 0);
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
		GetSettingsWindow(), *variable.get());
	if (accepted && oldName != variable->Name()) {
		VariableSignalManager::Instance()->Rename(
			QString::fromStdString(oldName),
			QString::fromStdString(variable->Name()));
	}
}

void VariableTable::Remove()
{
	auto selectedRows = Table()->selectionModel()->selectedRows();
	if (selectedRows.empty()) {
		return;
	}

	QStringList varNames;
	for (const auto &row : selectedRows) {
		auto cell = Table()->item(row.row(), 0);
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

void VariableTable::Filter()
{
	const auto itemMatches = [&](const QTableWidgetItem *item) {
		if (!item) {
			return false;
		}

		if (_settings.searchString.empty()) {
			return true;
		}

		const auto text = item->text();

		if (_settings.regex.Enabled()) {
			return _settings.regex.Matches(text.toStdString(),
						       _settings.searchString);
		}

		return text.contains(
			QString::fromStdString(_settings.searchString),
			Qt::CaseInsensitive);
	};

	for (int row = 0; row < Table()->rowCount(); ++row) {
		bool match = false;

		if (_settings.searchType == Settings::SearchType::ALL) {
			for (int col = 0; col < Table()->columnCount(); ++col) {
				if (itemMatches(Table()->item(row, col))) {
					match = true;
					break;
				}
			}
		} else {
			if (itemMatches(Table()->item(
				    row,
				    static_cast<int>(_settings.searchType)))) {
				match = true;
			}
		}

		Table()->setRowHidden(row, !match);
	}
}

VariableTable::VariableTable(Settings &settings, QWidget *parent)
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
		  [this]() { openSettingsDialog(this); }),
	  _searchField(new QLineEdit(this)),
	  _searchType(new QComboBox(this)),
	  _regexWidget(new RegexConfigWidget(this)),
	  _addDock(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.variableTab.addDock"))),
	  _settings(settings)
{
	for (const auto &variable : GetVariables()) {
		auto v = std::static_pointer_cast<Variable>(variable);
		AddItemTableRow(Table(), getCellLabels(v.get()));
	}

	connect(Table(), &QTableWidget::itemChanged, this,
		[this]() { Filter(); });

	_searchField->setPlaceholderText(obs_module_text(
		("AdvSceneSwitcher.variableTab.search.placeholder")));
	_searchField->setText(QString::fromStdString(_settings.searchString));
	connect(_searchField, &QLineEdit::textEdited, this,
		[this](const QString &text) {
			_settings.searchString = text.toStdString();
			Filter();
		});

	_searchType->addItem(
		obs_module_text("AdvSceneSwitcher.variableTab.search.all"),
		Settings::SearchType::ALL);
	_searchType->addItem(
		obs_module_text("AdvSceneSwitcher.variableTab.search.name"),
		Settings::SearchType::NAME);
	_searchType->addItem(
		obs_module_text("AdvSceneSwitcher.variableTab.search.value"),
		Settings::SearchType::VALUE);
	_searchType->setCurrentIndex(
		_searchType->findData(_settings.searchType));

	connect(_searchType, &QComboBox::currentIndexChanged, this, [this]() {
		_settings.searchType = static_cast<Settings::SearchType>(
			_searchType->currentData().toInt());
	});

	_regexWidget->SetRegexConfig(_settings.regex);
	connect(_regexWidget, &RegexConfigWidget::RegexConfigChanged, this,
		[this](const RegexConfig &regex) {
			_settings.regex = regex;
			Filter();
		});

	_addDock->setChecked(addDock);
	connect(_addDock, &QCheckBox::stateChanged, this,
		[this](int checked) { enableDock(checked); });

	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Rename, this,
			 [this](const QString &oldName,
				const QString &newName) {
				 RenameItemTableRow(Table(), oldName, newName);
			 });
	QWidget::connect(
		VariableSignalManager::Instance(), &VariableSignalManager::Add,
		this, [this](const QString &name) {
			AddItemTableRow(
				Table(),
				getCellLabels(GetVariableByQString(name)));
			SetHelpVisible(false);
			HighlightAddButton(false);
		});
	QWidget::connect(VariableSignalManager::Instance(),
			 &VariableSignalManager::Remove, this,
			 [this](const QString &name) {
				 RemoveItemTableRow(Table(), name);
				 if (Table()->rowCount() == 0) {
					 SetHelpVisible(true);
					 HighlightAddButton(true);
				 }
			 });

	auto timer = new QTimer(this);
	timer->setInterval(1000);
	QWidget::connect(timer, &QTimer::timeout,
			 [this]() { updateVariableStatus(Table()); });
	timer->start();

	auto searchLayout = new QHBoxLayout();
	searchLayout->addWidget(_searchField);
	searchLayout->addWidget(_searchType);
	searchLayout->addWidget(_regexWidget);
	searchLayout->addWidget(_addDock);
	qobject_cast<QVBoxLayout *>(layout())->insertLayout(0, searchLayout);

	SetHelpVisible(GetVariables().empty());
}

static void setupTab(QTabWidget *tab)
{
	if (GetVariables().empty()) {
		setTabVisible(tab, false);
	}

	QWidget::connect(
		VariableSignalManager::Instance(), &VariableSignalManager::Add,
		tab, [tab](const QString &name) { setTabVisible(tab, true); });
}

void VariableTable::Settings::Save(obs_data_t *data, const char *name)
{
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_int(settings, "searchType", static_cast<int>(searchType));
	obs_data_set_string(settings, "searchString", searchString.c_str());
	regex.Save(settings);
	obs_data_set_obj(data, name, settings);
}

void VariableTable::Settings::Load(obs_data_t *data, const char *name)
{
	OBSDataAutoRelease settings = obs_data_get_obj(data, name);
	searchType = static_cast<SearchType>(
		obs_data_get_int(settings, "searchType"));
	searchString = obs_data_get_string(settings, "searchString");
	regex.Load(settings);
}

} // namespace advss
