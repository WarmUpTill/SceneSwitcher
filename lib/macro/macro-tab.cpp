#include "advanced-scene-switcher.hpp"
#include "action-queue.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "macro-export-import-dialog.hpp"
#include "macro-settings.hpp"
#include "macro-search.hpp"
#include "macro-signals.hpp"
#include "macro-tree.hpp"
#include "macro.hpp"
#include "math-helpers.hpp"
#include "name-dialog.hpp"
#include "path-helpers.hpp"
#include "splitter-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "version.h"

#include <obs-frontend-api.h>
#include <QColor>
#include <QMenu>

namespace advss {

static QObject *addPulse = nullptr;
static QTimer onChangeHighlightTimer;

static void disableAddButtonHighlight()
{
	if (addPulse) {
		addPulse->deleteLater();
		addPulse = nullptr;
	}
}

static bool macroNameExists(const std::string &name)
{
	return !!GetMacroByName(name.c_str());
}

static bool newMacroNameIsValid(const std::string &name)
{
	if (!macroNameExists(name)) {
		return true;
	}

	auto macro = GetMacroByName(name.c_str());
	if (!macro) {
		return false;
	}

	QString fmt = obs_module_text(
		macro->IsGroup() ? "AdvSceneSwitcher.macroTab.groupNameExists"
				 : "AdvSceneSwitcher.macroTab.macroNameExists");

	DisplayMessage(fmt.arg(QString::fromStdString(name)));
	return false;
}

bool AdvSceneSwitcher::AddNewMacro(std::shared_ptr<Macro> &res,
				   std::string &name, std::string format)
{
	QString fmt;
	int i = 1;
	if (format.empty()) {
		fmt = QString(obs_module_text(
			"AdvSceneSwitcher.macroTab.defaultname"));
	} else {
		fmt = QString::fromStdString(format);
		i = 2;
	}

	QString placeHolderText = fmt.arg(i);
	while ((macroNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = fmt.arg(++i);
	}

	bool accepted = NameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.macroTab.add"),
		obs_module_text("AdvSceneSwitcher.macroTab.name"), name,
		placeHolderText);

	if (!accepted) {
		return false;
	}

	if (name.empty()) {
		return false;
	}

	if (!newMacroNameIsValid(name)) {
		return false;
	}

	res = std::make_shared<Macro>(name, GetGlobalMacroSettings());
	return true;
}

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	// Hotkey to add new macro will also use this function.
	// Since we don't want the hotkey to have an effect if the macro tab is
	// not focused we need to check this here.
	if (!MacroTabIsInFocus()) {
		return;
	}

	std::string name;
	std::shared_ptr<Macro> newMacro;
	if (!AddNewMacro(newMacro, name)) {
		return;
	}

	disableAddButtonHighlight();

	auto selectedMacro = GetSelectedMacro();
	if (!selectedMacro) {
		ui->macros->Add(newMacro);
		MacroSignalManager::Instance()->Add(
			QString::fromStdString(name));
		return;
	}

	if (selectedMacro->IsGroup()) {
		ui->macros->ExpandGroup(selectedMacro);
		Macro::PrepareMoveToGroup(selectedMacro, newMacro);
		ui->macros->AddToGroup(newMacro, selectedMacro);
		MacroSignalManager::Instance()->Add(
			QString::fromStdString(name));
		return;
	}

	auto selectedMacroGroup = selectedMacro->Parent();
	if (!selectedMacroGroup) {
		ui->macros->Add(newMacro, selectedMacro);
		MacroSignalManager::Instance()->Add(
			QString::fromStdString(name));
		return;
	}

	Macro::PrepareMoveToGroup(selectedMacroGroup, newMacro);
	ui->macros->Add(newMacro, selectedMacro);
	MacroSignalManager::Instance()->Add(QString::fromStdString(name));
}

static void addGroupSubitems(std::vector<std::shared_ptr<Macro>> &macros,
			     const std::shared_ptr<Macro> &group)
{
	auto subitems = GetGroupMacroEntries(group.get());

	// Remove subitems which were already selected to avoid duplicates
	for (const auto &subitem : subitems) {
		auto it = std::find(macros.begin(), macros.end(), subitem);
		if (it == macros.end()) {
			continue;
		}
		macros.erase(it);
	}

	// Add group subitems
	auto it = std::find(macros.begin(), macros.end(), group);
	if (it == macros.end()) {
		return;
	}
	it = std::next(it);
	macros.insert(it, subitems.begin(), subitems.end());
}

void AdvSceneSwitcher::RemoveMacro(std::shared_ptr<Macro> &macro)
{
	if (!macro) {
		return;
	}

	auto name = QString::fromStdString(macro->Name());
	if (macro->IsGroup() && macro->GroupSize() > 0) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.macroTab.removeGroupPopup.text");
		if (!DisplayMessage(deleteWarning.arg(name), true)) {
			return;
		}
	}

	if (macro->IsGroup()) {
		std::vector<std::shared_ptr<Macro>> macros = {macro};
		addGroupSubitems(macros, macro);
		for (auto macro : macros) {
			ui->macroEdit->ClearSegmentWidgetCacheFor(macro.get());
		}
	} else {
		ui->macroEdit->ClearSegmentWidgetCacheFor(macro.get());
	}

	// Don't cache widgets for about to be deleted macros
	MacroSegmentList::SetCachingEnabled(false);
	ui->macros->Remove(macro);
	MacroSegmentList::SetCachingEnabled(true);

	MacroSignalManager::Instance()->Remove(name);
}

void AdvSceneSwitcher::RemoveSelectedMacros()
{
	auto macros = GetSelectedMacros();
	if (macros.empty()) {
		return;
	}

	int macroCount = macros.size();
	if (macroCount == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.macroTab.removeSingleMacroPopup.text");
		auto &macro = macros.at(0);
		deleteWarning = deleteWarning.arg(
			QString::fromStdString(macro->Name()));

		if ((!macro->IsGroup() || macro->GroupSize() == 0) &&
		    !DisplayMessage(deleteWarning, true)) {
			return;
		}

		RemoveMacro(macro);
		return;
	}

	QString deleteWarning = obs_module_text(
		"AdvSceneSwitcher.macroTab.removeMultipleMacrosPopup.text");
	if (!DisplayMessage(deleteWarning.arg(macroCount), true)) {
		return;
	}

	for (auto &macro : macros) {
		RemoveMacro(macro);
	}
}

void AdvSceneSwitcher::RenameMacro(std::shared_ptr<Macro> &macro,
				   const QString &name)
{
	auto oldName = QString::fromStdString(macro->Name());
	{
		auto lock = LockContext();
		macro->SetName(name.toStdString());
	}

	MacroSignalManager::Instance()->Rename(oldName, name);
}

void AdvSceneSwitcher::on_macroRemove_clicked()
{
	RemoveSelectedMacros();
}

void AdvSceneSwitcher::on_macroUp_clicked() const
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	ui->macros->Up(macro);
}

void AdvSceneSwitcher::on_macroDown_clicked() const
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	ui->macros->Down(macro);
}

void AdvSceneSwitcher::RenameSelectedMacro()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	std::string oldName = macro->Name();
	std::string name;
	if (!NameDialog::AskForName(
		    this, obs_module_text("AdvSceneSwitcher.windowTitle"),
		    obs_module_text("AdvSceneSwitcher.item.newName"), name,
		    QString::fromStdString(oldName))) {
		return;
	}

	if (name.empty() || name == oldName || !newMacroNameIsValid(name)) {
		return;
	}

	RenameMacro(macro, QString::fromStdString(name));

	const QSignalBlocker b(ui->macroName);
	ui->macroName->setText(QString::fromStdString(name));
}

void AdvSceneSwitcher::ExportMacros() const
{
	auto selectedMacros = GetSelectedMacros();
	auto macros = selectedMacros;
	for (const auto &macro : selectedMacros) {
		if (macro->IsGroup() && macro->GroupSize() > 0) {
			addGroupSubitems(macros, macro);
		}
	}

	OBSDataAutoRelease data = obs_data_create();
	OBSDataArrayAutoRelease macroArray = obs_data_array_create();
	for (const auto &macro : macros) {
		OBSDataAutoRelease obj = obs_data_create();
		macro->Save(obj);
		obs_data_array_push_back(macroArray, obj);
	}
	obs_data_set_array(data, "macros", macroArray);
	SaveVariables(data);
	SaveActionQueues(data);
	obs_data_set_string(data, "version", g_GIT_TAG);
	auto json = obs_data_get_json(data);
	QString exportString(json);

	MacroExportImportDialog::ExportMacros(exportString);
}

bool AdvSceneSwitcher::ResolveMacroImportNameConflict(
	std::shared_ptr<Macro> &macro)
{
	QString errorMesg = obs_module_text(
		"AdvSceneSwitcher.macroTab.import.nameConflict");
	errorMesg = errorMesg.arg(QString::fromStdString(macro->Name()),
				  QString::fromStdString(macro->Name()));
	bool continueResolve = DisplayMessage(errorMesg, true);
	if (!continueResolve) {
		return false;
	}

	QString format = QString::fromStdString(macro->Name()) + " %1";
	int i = 2;

	QString placeHolderText = format.arg(i);
	while ((macroNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = format.arg(++i);
	}

	std::string newName;
	bool accepted = NameDialog::AskForName(
		this,
		obs_module_text(macro->IsGroup()
					? "AdvSceneSwitcher.macroTab.addGroup"
					: "AdvSceneSwitcher.macroTab.add"),
		obs_module_text("AdvSceneSwitcher.macroTab.name"), newName,
		placeHolderText);

	if (!accepted) {
		return false;
	}

	if (newName.empty()) {
		return false;
	}

	if (!newMacroNameIsValid(newName)) {
		return ResolveMacroImportNameConflict(macro);
	}

	macro->SetName(newName);
	return true;
}

void AdvSceneSwitcher::ImportMacros()
{
	QString json;
	if (!MacroExportImportDialog::ImportMacros(json)) {
		return;
	}
	OBSDataAutoRelease data =
		obs_data_create_from_json(json.toStdString().c_str());
	if (!data) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.macroTab.import.invalid"));
		ImportMacros();
		return;
	}
	ImportVariables(data);
	ImportQueues(data);

	auto version = obs_data_get_string(data, "version");
	if (strcmp(version, g_GIT_TAG) != 0) {
		blog(LOG_WARNING,
		     "importing macros from non matching plugin version \"%s\"",
		     version);
	}

	OBSDataArrayAutoRelease array = obs_data_get_array(data, "macros");
	size_t count = obs_data_array_count(array);

	int groupSize = 0;
	std::shared_ptr<Macro> group;
	std::vector<std::shared_ptr<Macro>> importedMacros;
	auto &tempMacros = GetTemporaryMacros();

	auto lock = LockContext();
	for (size_t i = 0; i < count; i++) {
		tempMacros.clear();

		OBSDataAutoRelease array_obj = obs_data_array_item(array, i);
		auto macro = std::make_shared<Macro>();
		tempMacros.emplace_back(macro);
		macro->Load(array_obj);
		RunAndClearPostLoadSteps();

		if (macroNameExists(macro->Name()) &&
		    !ResolveMacroImportNameConflict(macro)) {
			groupSize--;
			continue;
		}

		importedMacros.emplace_back(macro);
		GetTopLevelMacros().emplace_back(macro);
		if (groupSize > 0 && !macro->IsGroup()) {
			Macro::PrepareMoveToGroup(group, macro);
			groupSize--;
		}

		if (macro->IsGroup()) {
			group = macro;
			groupSize = macro->GroupSize();
			// We are not sure if all elements will be added so we
			// have to reset the group size to zero and add elements
			// to the group as they come up.
			macro->ResetGroupSize();
		}
	}

	for (const auto &macro : importedMacros) {
		macro->PostLoad();
	}
	RunAndClearPostLoadSteps();
	tempMacros.clear();

	ui->macros->Reset(GetTopLevelMacros(),
			  GetGlobalMacroSettings()._highlightExecuted);
	disableAddButtonHighlight();
}

void AdvSceneSwitcher::on_macroName_editingFinished()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	QString newName = ui->macroName->text();
	QString oldName = QString::fromStdString(macro->Name());

	if (newName.isEmpty() || newName == oldName ||
	    !newMacroNameIsValid(newName.toStdString())) {
		ui->macroName->setText(oldName);
		return;
	}
	RenameMacro(macro, newName);
}

void AdvSceneSwitcher::on_runMacroInParallel_stateChanged(int value) const
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	auto lock = LockContext();
	macro->SetRunInParallel(value);
}

void AdvSceneSwitcher::on_runMacroOnChange_stateChanged(int value) const
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	auto lock = LockContext();
	macro->SetMatchOnChange(value);
}

void AdvSceneSwitcher::SetMacroEditAreaDisabled(bool disable) const
{
	ui->macroName->setDisabled(disable);
	ui->runMacro->setDisabled(disable);
	ui->runMacroInParallel->setDisabled(disable);
	ui->runMacroOnChange->setDisabled(disable);
	ui->macroEdit->SetControlsDisabled(disable);
}

std::shared_ptr<Macro> AdvSceneSwitcher::GetSelectedMacro() const
{
	return ui->macros->GetCurrentMacro();
}

std::vector<std::shared_ptr<Macro>> AdvSceneSwitcher::GetSelectedMacros() const
{
	return ui->macros->GetCurrentMacros();
}

void AdvSceneSwitcher::MacroSelectionChanged()
{
	if (loading) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		SetMacroEditAreaDisabled(true);
		ui->macroEdit->SetMacro(macro);
		return;
	}

	{
		const QSignalBlocker b1(ui->macroName);
		const QSignalBlocker b2(ui->runMacroInParallel);
		const QSignalBlocker b3(ui->runMacroOnChange);
		ui->macroName->setText(macro->Name().c_str());
		ui->runMacroInParallel->setChecked(macro->RunInParallel());
		ui->runMacroOnChange->setChecked(macro->MatchOnChange());
	}

	macro->ResetUIHelpers();
	ui->macroEdit->SetMacro(macro);
	SetMacroEditAreaDisabled(false);

	if (macro->IsGroup()) {
		SetMacroEditAreaDisabled(true);
		ui->macroName->setEnabled(true);
		return;
	}

	if (GetGlobalMacroSettings()._saveSettingsOnMacroChange) {
		obs_frontend_save();
	}
}

void AdvSceneSwitcher::HighlightOnChange() const
{
	if (!GetGlobalMacroSettings()._highlightActions &&
	    !GetGlobalMacroSettings()._highlightExecuted) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (macro->OnChangePreventedActionsRecently()) {
		HighlightWidget(ui->runMacroOnChange, Qt::yellow,
				Qt::transparent, true);
	}
}

void AdvSceneSwitcher::on_macroSettings_clicked()
{
	GlobalMacroSettings prop = GetGlobalMacroSettings();
	bool accepted = MacroSettingsDialog::AskForSettings(
		this, prop, GetSelectedMacro().get());
	if (!accepted) {
		return;
	}

	GetGlobalMacroSettings() = prop;
	MacroSignalManager::Instance()->HighlightChanged(
		prop._highlightExecuted);

	// Reset highlights to avoid all macro segments being highlighted, which
	// would have been highlighted at least once since the moment the
	// highlighting was disabled
	if (prop._highlightConditions) {
		ui->macroEdit->ResetConditionHighlights();
	}
	if (prop._highlightActions) {
		ui->macroEdit->ResetActionHighlights();
	}

	SetCheckIntervalTooLowVisibility();
}

static bool shouldRestoreSplitter(const QList<int> &pos)
{
	if (pos.size() == 0) {
		return false;
	}

	for (int i = 0; i < pos.size(); ++i) {
		if (pos[i] == 0) {
			return false;
		}
	}
	return true;
}

static QToolBar *
setupToolBar(const std::initializer_list<std::initializer_list<QWidget *>>
		     &widgetGroups)
{
	auto toolbar = new QToolBar();
	toolbar->setIconSize({16, 16});

	QAction *lastSeparator = nullptr;

	for (const auto &widgetGroup : widgetGroups) {
		for (const auto &widget : widgetGroup) {
			if (!widget) {
				continue;
			}
			toolbar->addWidget(widget);
		}
		lastSeparator = toolbar->addSeparator();
	}

	if (lastSeparator) {
		toolbar->removeAction(lastSeparator);
	}

	// Prevent "extension" button from showing up
	toolbar->setSizePolicy(QSizePolicy::MinimumExpanding,
			       QSizePolicy::Preferred);
	return toolbar;
}

void AdvSceneSwitcher::SetupMacroTab()
{
	ui->macros->installEventFilter(this);

	auto &macros = GetTopLevelMacros();

	if (macros.size() == 0 && !switcher->disableHints) {
		addPulse = HighlightWidget(ui->macroAdd, QColor(Qt::green));
	}

	auto macroControls = setupToolBar({{ui->macroAdd, ui->macroRemove},
					   {ui->macroUp, ui->macroDown}});
	ui->macroControlLayout->addWidget(macroControls);

	ui->macros->Reset(macros, GetGlobalMacroSettings()._highlightExecuted);
	connect(ui->macros, SIGNAL(MacroSelectionChanged()), this,
		SLOT(MacroSelectionChanged()));
	ui->runMacro->SetMacroTree(ui->macros);

	ui->macros->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macros, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroContextMenu);

	SetMacroEditAreaDisabled(true);
	ui->macroPriorityWarning->setVisible(
		switcher->functionNamesByPriority[0] != macro_func);

	onChangeHighlightTimer.setInterval(1500);
	connect(&onChangeHighlightTimer, SIGNAL(timeout()), this,
		SLOT(HighlightOnChange()));
	onChangeHighlightTimer.start();

	// Reserve more space for macro edit area than for the macro list
	ui->macroListMacroEditSplitter->setStretchFactor(0, 1);
	ui->macroListMacroEditSplitter->setStretchFactor(1, 4);

	if (switcher->saveWindowGeo) {
		if (shouldRestoreSplitter(
			    switcher->macroListMacroEditSplitterPosition)) {
			ui->macroListMacroEditSplitter->setSizes(
				switcher->macroListMacroEditSplitterPosition);
		}
	}

	SetupMacroSearchWidgets(ui->macroSearchLayout, ui->macroSearchText,
				ui->macroSearchClear, ui->macroSearchType,
				ui->macroSearchRegex,
				ui->macroSearchShowSettings,
				[this]() { ui->macros->RefreshFilter(); });
}

void AdvSceneSwitcher::ShowMacroContextMenu(const QPoint &pos)
{
	QPoint globalPos = ui->macros->mapToGlobal(pos);
	QMenu menu;

	menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.contextMenuAdd"),
		this, &AdvSceneSwitcher::on_macroAdd_clicked);

	auto copy = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.copy"), this,
		&AdvSceneSwitcher::CopyMacro);
	copy->setEnabled(ui->macros->SingleItemSelected() &&
			 !ui->macros->GroupsSelected());
	menu.addSeparator();

	auto rename = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.rename"), this,
		&AdvSceneSwitcher::RenameSelectedMacro);
	rename->setEnabled(ui->macros->SingleItemSelected());

	auto remove = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.remove"), this,
		&AdvSceneSwitcher::on_macroRemove_clicked);
	remove->setDisabled(ui->macros->SelectionEmpty());
	menu.addSeparator();

	auto group = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.group"), ui->macros,
		&MacroTree::GroupSelectedItems);
	group->setDisabled(ui->macros->GroupedItemsSelected() ||
			   ui->macros->GroupsSelected() ||
			   ui->macros->SelectionEmpty());

	auto ungroup = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.ungroup"),
		ui->macros, &MacroTree::UngroupSelectedGroups);
	ungroup->setEnabled(ui->macros->GroupsSelected());

	auto expandAll = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.expandAllGroups"),
		ui->macros, &MacroTree::ExpandAll);
	expandAll->setEnabled(ui->macros->GroupsExist());

	auto collapseAll = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.collapseAllGroups"),
		ui->macros, &MacroTree::CollapseAll);
	collapseAll->setEnabled(ui->macros->GroupsExist());
	menu.addSeparator();

	auto exportAction = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.export"), this,
		&AdvSceneSwitcher::ExportMacros);
	exportAction->setDisabled(ui->macros->SelectionEmpty());
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.import"),
		       this, &AdvSceneSwitcher::ImportMacros);

	menu.exec(globalPos);
}

void AdvSceneSwitcher::CopyMacro()
{
	const auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	std::string format = macro->Name() + " %1";
	std::string name;
	std::shared_ptr<Macro> newMacro;
	if (!AddNewMacro(newMacro, name, format)) {
		return;
	}

	OBSDataAutoRelease data = obs_data_create();
	macro->Save(data, true);
	newMacro->Load(data);
	newMacro->PostLoad();
	newMacro->SetName(name);
	RunAndClearPostLoadSteps();
	Macro::PrepareMoveToGroup(macro->Parent(), newMacro);

	ui->macros->Add(newMacro, macro);
	disableAddButtonHighlight();
	MacroSignalManager::Instance()->Add(QString::fromStdString(name));
}

bool MacroTabIsInFocus()
{
	return AdvSceneSwitcher::window &&
	       AdvSceneSwitcher::window->MacroTabIsInFocus();
}

bool AdvSceneSwitcher::MacroTabIsInFocus()
{
	return isActiveWindow() && isAncestorOf(focusWidget()) &&
	       (ui->tabWidget->currentWidget()->objectName() == "macroTab");
}

} // namespace advss
