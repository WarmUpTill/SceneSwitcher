#include "advanced-scene-switcher.hpp"
#include "action-queue.hpp"
#include "cursor-shape-changer.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "macro-export-import-dialog.hpp"
#include "macro-settings.hpp"
#include "macro-segment-copy-paste.hpp"
#include "macro-tree.hpp"
#include "macro.hpp"
#include "math-helpers.hpp"
#include "name-dialog.hpp"
#include "path-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"
#include "version.h"

#include <obs-frontend-api.h>
#include <QColor>
#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPropertyAnimation>

namespace advss {

static QObject *addPulse = nullptr;
static QTimer onChangeHighlightTimer;

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

	res = std::make_shared<Macro>(
		name, GetGlobalMacroSettings()._newMacroRegisterHotkeys,
		GetGlobalMacroSettings()._newMacroUseShortCircuitEvaluation);
	return true;
}

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	std::string name;
	std::shared_ptr<Macro> newMacro;
	if (!AddNewMacro(newMacro, name)) {
		return;
	}

	ui->macros->Add(newMacro);
	if (addPulse) {
		addPulse->deleteLater();
		addPulse = nullptr;
	}
	emit MacroAdded(QString::fromStdString(name));
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

	ui->macros->Remove(macro);
	emit MacroRemoved(name);
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
		auto macro = macros.at(0);
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
	emit MacroRenamed(oldName, name);
}

void AdvSceneSwitcher::on_macroRemove_clicked()
{
	RemoveSelectedMacros();
}

void AdvSceneSwitcher::on_macroUp_clicked()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	ui->macros->Up(macro);
}

void AdvSceneSwitcher::on_macroDown_clicked()
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

static void addGroupSubitems(std::vector<std::shared_ptr<Macro>> &macros,
			     const std::shared_ptr<Macro> &group)
{
	std::vector<std::shared_ptr<Macro>> subitems;
	subitems.reserve(group->GroupSize());

	// Find all subitems
	auto allMacros = GetMacros();
	for (auto it = allMacros.begin(); it < allMacros.end(); it++) {
		if ((*it)->Name() == group->Name()) {
			for (uint32_t i = 1; i <= group->GroupSize(); i++) {
				subitems.emplace_back(*std::next(it, i));
			}
			break;
		}
	}

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

void AdvSceneSwitcher::ExportMacros()
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

static bool
isValidMacroSegmentIdx(const std::deque<std::shared_ptr<MacroSegment>> &list,
		       int idx)
{
	return (idx > 0 || (unsigned)idx < list.size());
}

void AdvSceneSwitcher::SetupMacroSegmentSelection(MacroSection type, int idx)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	MacroSegmentList *setList = nullptr, *resetList1 = nullptr,
			 *resetList2 = nullptr;
	int *setIdx = nullptr, *resetIdx1 = nullptr, *resetIdx2 = nullptr;
	std::deque<std::shared_ptr<MacroSegment>> segements;

	switch (type) {
	case AdvSceneSwitcher::MacroSection::CONDITIONS:
		setList = ui->conditionsList;
		setIdx = &currentConditionIdx;
		segements = {macro->Conditions().begin(),
			     macro->Conditions().end()};

		resetList1 = ui->actionsList;
		resetList2 = ui->elseActionsList;
		resetIdx1 = &currentActionIdx;
		resetIdx2 = &currentElseActionIdx;
		break;
	case AdvSceneSwitcher::MacroSection::ACTIONS:
		setList = ui->actionsList;
		setIdx = &currentActionIdx;
		segements = {macro->Actions().begin(), macro->Actions().end()};

		resetList1 = ui->conditionsList;
		resetList2 = ui->elseActionsList;
		resetIdx1 = &currentConditionIdx;
		resetIdx2 = &currentElseActionIdx;
		break;
	case AdvSceneSwitcher::MacroSection::ELSE_ACTIONS:
		setList = ui->elseActionsList;
		setIdx = &currentElseActionIdx;
		segements = {macro->ElseActions().begin(),
			     macro->ElseActions().end()};

		resetList1 = ui->actionsList;
		resetList2 = ui->conditionsList;
		resetIdx1 = &currentActionIdx;
		resetIdx2 = &currentConditionIdx;
		break;
	default:
		break;
	}

	setList->SetSelection(idx);
	resetList1->SetSelection(-1);
	resetList2->SetSelection(-1);
	if (isValidMacroSegmentIdx(segements, idx)) {
		*setIdx = idx;
	} else {
		*setIdx = -1;
	}
	*resetIdx1 = -1;
	*resetIdx2 = -1;

	lastInteracted = type;
	HighlightControls();
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

	auto lock = LockContext();
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease array_obj = obs_data_array_item(array, i);
		auto macro = std::make_shared<Macro>();
		macro->Load(array_obj);
		RunPostLoadSteps();

		if (macroNameExists(macro->Name()) &&
		    !ResolveMacroImportNameConflict(macro)) {
			groupSize--;
			continue;
		}

		importedMacros.emplace_back(macro);
		GetMacros().emplace_back(macro);
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
	RunPostLoadSteps();

	ui->macros->Reset(GetMacros(),
			  GetGlobalMacroSettings()._highlightExecuted);
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

void AdvSceneSwitcher::on_runMacroInParallel_stateChanged(int value)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	auto lock = LockContext();
	macro->SetRunInParallel(value);
}

void AdvSceneSwitcher::on_runMacroOnChange_stateChanged(int value)
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	auto lock = LockContext();
	macro->SetMatchOnChange(value);
}

void AdvSceneSwitcher::PopulateMacroActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.Actions();
	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		ui->actionsList->Add(newEntry);
	}
	ui->actionsList->SetHelpMsgVisible(actions.size() == 0);
}

void AdvSceneSwitcher::PopulateMacroElseActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.ElseActions();
	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		ui->elseActionsList->Add(newEntry);
	}
	ui->elseActionsList->SetHelpMsgVisible(actions.size() == 0);
}

void AdvSceneSwitcher::PopulateMacroConditions(Macro &m, uint32_t afterIdx)
{
	bool root = afterIdx == 0;
	auto &conditions = m.Conditions();
	for (; afterIdx < conditions.size(); afterIdx++) {
		auto newEntry = new MacroConditionEdit(
			this, &conditions[afterIdx],
			conditions[afterIdx]->GetId(), root);
		ui->conditionsList->Add(newEntry);
		root = false;
	}
	ui->conditionsList->SetHelpMsgVisible(conditions.size() == 0);
}

void AdvSceneSwitcher::SetActionData(Macro &m)
{
	auto &actions = m.Actions();
	for (int idx = 0; idx < ui->actionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->actionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroActionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(actions.begin() + idx));
	}
}

void AdvSceneSwitcher::SetElseActionData(Macro &m)
{
	auto &actions = m.ElseActions();
	for (int idx = 0; idx < ui->elseActionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->elseActionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroActionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(actions.begin() + idx));
	}
}

void AdvSceneSwitcher::SetConditionData(Macro &m)
{
	auto &conditions = m.Conditions();
	for (int idx = 0; idx < ui->conditionsList->ContentLayout()->count();
	     idx++) {
		auto item = ui->conditionsList->ContentLayout()->itemAt(idx);
		if (!item) {
			continue;
		}
		auto widget = static_cast<MacroConditionEdit *>(item->widget());
		if (!widget) {
			continue;
		}
		widget->SetEntryData(&*(conditions.begin() + idx));
	}
}

static void maximizeFirstSplitterEntry(QSplitter *splitter)
{
	QList<int> newSizes;
	newSizes << 999999;
	for (int i = 0; i < splitter->sizes().size() - 1; i++) {
		newSizes << 0;
	}
	splitter->setSizes(newSizes);
}

static void centerSplitterPosition(QSplitter *splitter)
{
	splitter->setSizes(QList<int>() << 999999 << 999999);
}

void AdvSceneSwitcher::SetEditMacro(Macro &m)
{
	{
		const QSignalBlocker b1(ui->macroName);
		const QSignalBlocker b2(ui->runMacroInParallel);
		const QSignalBlocker b3(ui->runMacroOnChange);
		ui->macroName->setText(m.Name().c_str());
		ui->runMacroInParallel->setChecked(m.RunInParallel());
		ui->runMacroOnChange->setChecked(m.MatchOnChange());
	}
	ui->conditionsList->Clear();
	ui->actionsList->Clear();
	ui->elseActionsList->Clear();

	m.ResetUIHelpers();

	PopulateMacroConditions(m);
	PopulateMacroActions(m);
	PopulateMacroElseActions(m);
	SetMacroEditAreaDisabled(false);

	currentActionIdx = -1;
	currentElseActionIdx = -1;
	currentConditionIdx = -1;
	HighlightControls();

	if (m.IsGroup()) {
		SetMacroEditAreaDisabled(true);
		ui->macroName->setEnabled(true);
		centerSplitterPosition(ui->macroActionConditionSplitter);
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		return;
	}

	if (!m.HasValidSplitterPositions()) {
		centerSplitterPosition(ui->macroActionConditionSplitter);
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		return;
	}

	ui->macroActionConditionSplitter->setSizes(
		m.GetActionConditionSplitterPosition());
	ui->macroElseActionSplitter->setSizes(
		m.GetElseActionSplitterPosition());
}

void AdvSceneSwitcher::SetMacroEditAreaDisabled(bool disable)
{
	ui->macroName->setDisabled(disable);
	ui->runMacro->setDisabled(disable);
	ui->runMacroInParallel->setDisabled(disable);
	ui->runMacroOnChange->setDisabled(disable);
	ui->macroActions->setDisabled(disable);
	ui->macroConditions->setDisabled(disable);
	ui->macroActionConditionSplitter->setDisabled(disable);
}

void AdvSceneSwitcher::HighlightAction(int idx, QColor color)
{
	ui->actionsList->Highlight(idx, color);
}

void AdvSceneSwitcher::HighlightElseAction(int idx, QColor color)
{
	ui->elseActionsList->Highlight(idx, color);
}

void AdvSceneSwitcher::HighlightCondition(int idx, QColor color)
{
	ui->conditionsList->Highlight(idx, color);
}

std::shared_ptr<Macro> AdvSceneSwitcher::GetSelectedMacro()
{
	return ui->macros->GetCurrentMacro();
}

std::vector<std::shared_ptr<Macro>> AdvSceneSwitcher::GetSelectedMacros()
{
	return ui->macros->GetCurrentMacros();
}

void AdvSceneSwitcher::MacroSelectionAboutToChange()
{
	if (loading) {
		return;
	}

	if (!ui->macroName->isEnabled()) { // No macro is selected
		return;
	}

	auto macro = GetMacroByQString(ui->macroName->text());
	if (!macro) {
		return;
	}

	macro->SetActionConditionSplitterPosition(
		ui->macroActionConditionSplitter->sizes());

	auto elsePos = ui->macroElseActionSplitter->sizes();
	// If only conditions are visible maximize the actions to avoid neither
	// actions nor elseActions being visible when the condition <-> action
	// splitter is moved
	if (elsePos[0] == 0 && elsePos[1] == 0) {
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		return;
	}
	macro->SetElseActionSplitterPosition(
		ui->macroElseActionSplitter->sizes());
}

void AdvSceneSwitcher::MacroSelectionChanged()
{
	if (loading) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		SetMacroEditAreaDisabled(true);
		ui->conditionsList->Clear();
		ui->actionsList->Clear();
		ui->elseActionsList->Clear();
		ui->conditionsList->SetHelpMsgVisible(true);
		ui->actionsList->SetHelpMsgVisible(true);
		ui->elseActionsList->SetHelpMsgVisible(true);
		centerSplitterPosition(ui->macroActionConditionSplitter);
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		return;
	}
	SetEditMacro(*macro);
	obs_frontend_save();
}

void AdvSceneSwitcher::HighlightOnChange()
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

static void resetSegmentHighlights(MacroSegmentList *list)
{
	MacroSegmentEdit *widget = nullptr;
	for (int i = 0; (widget = list->WidgetAt(i)); i++) {
		if (widget && widget->Data()) {
			(void)widget->Data()->GetHighlightAndReset();
		}
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
	emit HighlightMacrosChanged(prop._highlightExecuted);

	// Reset highlights to avoid all macro segments being highlighted, which
	// would have been highlighted at least once since the moment the
	// highlighting was disabled
	if (prop._highlightConditions) {
		resetSegmentHighlights(ui->conditionsList);
	}
	if (prop._highlightActions) {
		resetSegmentHighlights(ui->actionsList);
		resetSegmentHighlights(ui->elseActionsList);
	}

	SetCheckIntervalTooLowVisibility();
}

static void moveControlsToSplitter(QSplitter *splitter, int idx,
				   QLayoutItem *item)
{
	static int splitterHandleWidth = 32;
	auto handle = splitter->handle(idx);
	auto layout = item->layout();
	int leftMargin, rightMargin;
	layout->getContentsMargins(&leftMargin, nullptr, &rightMargin, nullptr);
	layout->setContentsMargins(leftMargin, 0, rightMargin, 0);
	handle->setLayout(layout);
	splitter->setHandleWidth(splitterHandleWidth);
	splitter->setStyleSheet("QSplitter::handle {background: transparent;}");
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

static void runSegmentHighligtChecksHelper(MacroSegmentList *list)
{
	MacroSegmentEdit *widget = nullptr;
	for (int i = 0; (widget = list->WidgetAt(i)); i++) {
		if (widget->Data() && widget->Data()->GetHighlightAndReset()) {
			list->Highlight(i);
		}
	}
}

static void runSegmentHighligtChecks(AdvSceneSwitcher *ss)
{
	if (!ss || !HighlightUIElementsEnabled()) {
		return;
	}
	auto macro = ss->GetSelectedMacro();
	if (!macro) {
		return;
	}

	const auto &settings = GetGlobalMacroSettings();
	if (settings._highlightConditions) {
		runSegmentHighligtChecksHelper(ss->ui->conditionsList);
	}
	if (settings._highlightActions) {
		runSegmentHighligtChecksHelper(ss->ui->actionsList);
		runSegmentHighligtChecksHelper(ss->ui->elseActionsList);
	}
}

static QToolBar *
setupToolBar(const std::initializer_list<std::initializer_list<QWidget *>>
		     &widgetGroups)
{
	auto toolbar = new QToolBar();
	toolbar->setIconSize({16, 16});

	QAction *lastSeperator = nullptr;

	for (const auto &widgetGroup : widgetGroups) {
		for (const auto &widget : widgetGroup) {
			toolbar->addWidget(widget);
		}
		lastSeperator = toolbar->addSeparator();
	}

	if (lastSeperator) {
		toolbar->removeAction(lastSeperator);
	}

	// Prevent "extension" button from showing up
	toolbar->setSizePolicy(QSizePolicy::MinimumExpanding,
			       QSizePolicy::Preferred);
	return toolbar;
}

void AdvSceneSwitcher::SetupMacroTab()
{
	ui->macroElseActions->installEventFilter(this);
	ui->macros->installEventFilter(this);

	if (GetMacros().size() == 0 && !switcher->disableHints) {
		addPulse = HighlightWidget(ui->macroAdd, QColor(Qt::green));
	}

	auto macroControls = setupToolBar({{ui->macroAdd, ui->macroRemove},
					   {ui->macroUp, ui->macroDown}});
	ui->macroControlLayout->addWidget(macroControls);

	ui->macros->Reset(GetMacros(),
			  GetGlobalMacroSettings()._highlightExecuted);
	connect(ui->macros, SIGNAL(MacroSelectionAboutToChange()), this,
		SLOT(MacroSelectionAboutToChange()));
	connect(ui->macros, SIGNAL(MacroSelectionChanged()), this,
		SLOT(MacroSelectionChanged()));
	ui->runMacro->SetMacroTree(ui->macros);

	ui->conditionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editConditionHelp"));
	connect(ui->conditionsList, &MacroSegmentList::SelectionChanged, this,
		&AdvSceneSwitcher::MacroConditionSelectionChanged);
	connect(ui->conditionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroConditionReorder);

	ui->actionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editActionHelp"));
	connect(ui->actionsList, &MacroSegmentList::SelectionChanged, this,
		&AdvSceneSwitcher::MacroActionSelectionChanged);
	connect(ui->actionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroActionReorder);

	ui->elseActionsList->SetHelpMsg(obs_module_text(
		"AdvSceneSwitcher.macroTab.editElseActionHelp"));
	connect(ui->elseActionsList, &MacroSegmentList::SelectionChanged, this,
		&AdvSceneSwitcher::MacroElseActionSelectionChanged);
	connect(ui->elseActionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroElseActionReorder);

	ui->macros->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macros, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroContextMenu);
	ui->actionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->actionsList, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroActionsContextMenu);
	ui->elseActionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->elseActionsList, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroElseActionsContextMenu);
	ui->conditionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->conditionsList, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroConditionsContextMenu);

	SetMacroEditAreaDisabled(true);
	ui->macroPriorityWarning->setVisible(
		switcher->functionNamesByPriority[0] != macro_func);

	onChangeHighlightTimer.setInterval(1500);
	connect(&onChangeHighlightTimer, SIGNAL(timeout()), this,
		SLOT(HighlightOnChange()));
	onChangeHighlightTimer.start();

	// Set action and condition toolbars
	const std::string pathPrefix =
		GetDataFilePath("res/images/" + GetThemeTypeName());
	SetButtonIcon(ui->actionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->actionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->elseActionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->elseActionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->conditionTop, (pathPrefix + "DoubleUp.svg").c_str());
	SetButtonIcon(ui->conditionBottom,
		      (pathPrefix + "DoubleDown.svg").c_str());
	SetButtonIcon(ui->toggleElseActions,
		      (pathPrefix + "NotEqual.svg").c_str());

	auto conditionToolbar =
		setupToolBar({{ui->conditionAdd, ui->conditionRemove},
			      {ui->conditionTop, ui->conditionUp,
			       ui->conditionDown, ui->conditionBottom}});
	auto actionToolbar = setupToolBar({{ui->actionAdd, ui->actionRemove},
					   {ui->actionTop, ui->actionUp,
					    ui->actionDown, ui->actionBottom}});
	auto elseActionToolbar =
		setupToolBar({{ui->elseActionAdd, ui->elseActionRemove},
			      {ui->elseActionTop, ui->elseActionUp,
			       ui->elseActionDown, ui->elseActionBottom}});

	ui->conditionControlsLayout->addWidget(conditionToolbar);
	ui->actionControlsLayout->insertWidget(0, actionToolbar);
	ui->elseActionControlsLayout->addWidget(elseActionToolbar);

	// Move condition controls into splitter handle layout
	moveControlsToSplitter(ui->macroActionConditionSplitter, 1,
			       ui->macroConditionsLayout->takeAt(1));
	moveControlsToSplitter(ui->macroElseActionSplitter, 1,
			       ui->macroActionsLayout->takeAt(1));

	// Override splitter cursor icon when hovering over controls in splitter
	SetCursorOnWidgetHover(ui->conditionAdd, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionRemove,
			       Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionTop, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionUp, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionDown, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->conditionBottom,
			       Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionAdd, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionRemove, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionTop, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionUp, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionDown, Qt::CursorShape::ArrowCursor);
	SetCursorOnWidgetHover(ui->actionBottom, Qt::CursorShape::ArrowCursor);

	// Reserve more space for macro edit area than for the macro list
	ui->macroListMacroEditSplitter->setStretchFactor(0, 1);
	ui->macroListMacroEditSplitter->setStretchFactor(1, 4);

	centerSplitterPosition(ui->macroActionConditionSplitter);
	maximizeFirstSplitterEntry(ui->macroElseActionSplitter);

	if (switcher->saveWindowGeo) {
		if (shouldRestoreSplitter(
			    switcher->macroListMacroEditSplitterPosition)) {
			ui->macroListMacroEditSplitter->setSizes(
				switcher->macroListMacroEditSplitterPosition);
		}
	}

	SetupSegmentCopyPasteShortcutHandlers(this);

	// Macro segment highlight
	auto timer = new QTimer(this);
	connect(timer, &QTimer::timeout,
		[this]() { runSegmentHighligtChecks(this); });
	timer->start(1500);
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

	auto exportAction = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.export"), this,
		&AdvSceneSwitcher::ExportMacros);
	exportAction->setDisabled(ui->macros->SelectionEmpty());
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.import"),
		       this, &AdvSceneSwitcher::ImportMacros);

	menu.exec(globalPos);
}

static bool handleCustomLabelRename(MacroSegmentEdit *segmentEdit)
{
	std::string label;
	auto segment = segmentEdit->Data();
	if (!segment) {
		return false;
	}

	bool accepted = NameDialog::AskForName(
		GetSettingsWindow(),
		obs_module_text(
			"AdvSceneSwitcher.macroTab.segment.setCustomLabel"),
		"", label, QString::fromStdString(segment->GetCustomLabel()));
	if (!accepted) {
		return false;
	}

	segment->SetCustomLabel(label);
	segmentEdit->HeaderInfoChanged("");
	return true;
}

static void handleCustomLabelEnableChange(MacroSegmentEdit *segmentEdit,
					  QAction *contextMenuOption)
{
	bool enable = contextMenuOption->isChecked();
	auto segment = segmentEdit->Data();
	segment->SetUseCustomLabel(enable);
	if (!enable) {
		segmentEdit->HeaderInfoChanged(
			QString::fromStdString(segment->GetShortDesc()));
		return;
	}

	if (!handleCustomLabelRename(segmentEdit)) {
		segment->SetUseCustomLabel(false);
	}
}

static void setupSegmentLabelContextMenuEntries(MacroSegmentEdit *segmentEdit,
						QMenu &menu)
{
	if (!segmentEdit) {
		return;
	}

	auto segment = segmentEdit ? segmentEdit->Data() : nullptr;
	const bool customLabelIsEnabled = segment &&
					  segment->GetUseCustomLabel();

	auto enableCustomLabel = menu.addAction(obs_module_text(
		"AdvSceneSwitcher.macroTab.segment.useCustomLabel"));
	enableCustomLabel->setCheckable(true);
	enableCustomLabel->setChecked(customLabelIsEnabled);
	QWidget::connect(enableCustomLabel, &QAction::triggered,
			 [segmentEdit, enableCustomLabel]() {
				 handleCustomLabelEnableChange(
					 segmentEdit, enableCustomLabel);
			 });

	if (!customLabelIsEnabled) {
		return;
	}

	auto customLabelRename = menu.addAction(obs_module_text(
		"AdvSceneSwitcher.macroTab.segment.customLabelRename"));
	QWidget::connect(customLabelRename, &QAction::triggered,
			 [segmentEdit]() {
				 handleCustomLabelRename(segmentEdit);
			 });
}

static void setupCopyPasteContextMenuEnry(AdvSceneSwitcher *ss,
					  MacroSegmentEdit *segmentEdit,
					  QMenu &menu)
{
	auto copy = menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.segment.copy"), ss,
		[ss]() { ss->CopyMacroSegment(); });
	copy->setEnabled(!!segmentEdit);

	const char *pasteText = "AdvSceneSwitcher.macroTab.segment.paste";
	if (MacroActionIsInClipboard()) {
		if (IsCursorInWidgetArea(ss->ui->macroActions)) {
			pasteText =
				"AdvSceneSwitcher.macroTab.segment.pasteAction";
		} else if (IsCursorInWidgetArea(ss->ui->macroElseActions)) {
			pasteText =
				"AdvSceneSwitcher.macroTab.segment.pasteElseAction";
		}
	}
	auto paste = menu.addAction(obs_module_text(pasteText), ss,
				    [ss]() { ss->PasteMacroSegment(); });
	paste->setEnabled(MacroSegmentIsInClipboard());
}

static void setupRemoveContextMenuEnry(
	AdvSceneSwitcher *ss,
	const std::function<void(AdvSceneSwitcher *, int)> &remove,
	const QPoint &pos, MacroSegmentList *list, QMenu &menu)
{
	const auto segmentEditIndex = list->IndexAt(pos);
	if (segmentEditIndex == -1) {
		return;
	}

	menu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.segment.remove"), ss,
		[ss, remove, segmentEditIndex]() {
			remove(ss, segmentEditIndex);
		});
}

static void setupConextMenu(AdvSceneSwitcher *ss, const QPoint &pos,
			    std::function<void(AdvSceneSwitcher *, int)> remove,
			    std::function<void(AdvSceneSwitcher *)> expand,
			    std::function<void(AdvSceneSwitcher *)> collapse,
			    std::function<void(AdvSceneSwitcher *)> maximize,
			    std::function<void(AdvSceneSwitcher *)> minimize,
			    MacroSegmentList *list)
{
	QMenu menu;
	auto segmentEdit = list->WidgetAt(pos);

	setupCopyPasteContextMenuEnry(ss, segmentEdit, menu);
	menu.addSeparator();
	setupRemoveContextMenuEnry(ss, remove, pos, list, menu);
	menu.addSeparator();
	setupSegmentLabelContextMenuEntries(segmentEdit, menu);
	menu.addSeparator();
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
		       ss, [ss, expand]() { expand(ss); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"),
		       ss, [ss, collapse]() { collapse(ss); });
	menu.addSeparator();
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
		       ss, [ss, maximize]() { maximize(ss); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
		       ss, [ss, minimize]() { minimize(ss); });
	menu.exec(list->mapToGlobal(pos));
}

void AdvSceneSwitcher::ShowMacroActionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, pos, &AdvSceneSwitcher::RemoveMacroAction,
			&AdvSceneSwitcher::ExpandAllActions,
			&AdvSceneSwitcher::CollapseAllActions,
			&AdvSceneSwitcher::MaximizeActions,
			&AdvSceneSwitcher::MinimizeActions, ui->actionsList);
}

void AdvSceneSwitcher::ShowMacroElseActionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, pos, &AdvSceneSwitcher::RemoveMacroElseAction,
			&AdvSceneSwitcher::ExpandAllElseActions,
			&AdvSceneSwitcher::CollapseAllElseActions,
			&AdvSceneSwitcher::MaximizeElseActions,
			&AdvSceneSwitcher::MinimizeElseActions,
			ui->elseActionsList);
}

void AdvSceneSwitcher::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, pos, &AdvSceneSwitcher::RemoveMacroCondition,
			&AdvSceneSwitcher::ExpandAllConditions,
			&AdvSceneSwitcher::CollapseAllConditions,
			&AdvSceneSwitcher::MaximizeConditions,
			&AdvSceneSwitcher::MinimizeConditions,
			ui->conditionsList);
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
	RunPostLoadSteps();
	Macro::PrepareMoveToGroup(macro->Parent(), newMacro);

	ui->macros->Add(newMacro, macro);
	if (addPulse) {
		addPulse->deleteLater();
		addPulse = nullptr;
	}
	emit MacroAdded(QString::fromStdString(name));
}

void setCollapsedHelper(const std::shared_ptr<Macro> &m, MacroSegmentList *list,
			bool collapsed)
{
	if (!m) {
		return;
	}
	list->SetCollapsed(collapsed);
}

void AdvSceneSwitcher::ExpandAllActions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->actionsList, false);
}

void AdvSceneSwitcher::ExpandAllElseActions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->elseActionsList, false);
}

void AdvSceneSwitcher::ExpandAllConditions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->conditionsList, false);
}

void AdvSceneSwitcher::CollapseAllActions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->actionsList, true);
}

void AdvSceneSwitcher::CollapseAllElseActions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->elseActionsList, true);
}

void AdvSceneSwitcher::CollapseAllConditions()
{
	setCollapsedHelper(GetSelectedMacro(), ui->conditionsList, true);
}

static void reduceSizeOfSplitterIdx(QSplitter *splitter, int idx)
{
	auto sizes = splitter->sizes();
	int sum = sizes[0] + sizes[1];
	int reducedSize = sum / 10;
	sizes[idx] = reducedSize;
	sizes[(idx + 1) % 2] = sum - reducedSize;
	splitter->setSizes(sizes);
}

void AdvSceneSwitcher::MinimizeActions()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	if (macro->ElseActions().size() == 0) {
		reduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 1);
	} else {
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
		reduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 1);
	}
}

void AdvSceneSwitcher::MaximizeActions()
{
	MinimizeElseActions();
	MinimizeConditions();
}

void AdvSceneSwitcher::MinimizeElseActions()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	if (macro->ElseActions().size() == 0) {
		maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
	} else {
		reduceSizeOfSplitterIdx(ui->macroElseActionSplitter, 1);
	}
}

void AdvSceneSwitcher::MaximizeElseActions()
{
	MinimizeConditions();
	reduceSizeOfSplitterIdx(ui->macroElseActionSplitter, 0);
}

void AdvSceneSwitcher::MinimizeConditions()
{
	reduceSizeOfSplitterIdx(ui->macroActionConditionSplitter, 0);
}

void AdvSceneSwitcher::MaximizeConditions()
{
	MinimizeElseActions();
	MinimizeActions();
}

void AdvSceneSwitcher::on_toggleElseActions_clicked()
{
	auto elsePosition = ui->macroElseActionSplitter->sizes();
	if (elsePosition[1] == 0) {
		centerSplitterPosition(ui->macroElseActionSplitter);
		return;
	}

	maximizeFirstSplitterEntry(ui->macroElseActionSplitter);
}

void AdvSceneSwitcher::SetElseActionsStateToHidden()
{
	ui->toggleElseActions->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.toggleElseActions.show.tooltip"));
	ui->toggleElseActions->setChecked(false);
}

void AdvSceneSwitcher::SetElseActionsStateToVisible()
{
	ui->toggleElseActions->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.toggleElseActions.hide.tooltip"));
	ui->toggleElseActions->setChecked(true);
}

bool AdvSceneSwitcher::MacroTabIsInFocus()
{
	return isActiveWindow() && isAncestorOf(focusWidget()) &&
	       (ui->tabWidget->currentWidget()->objectName() == "macroTab");
}

void AdvSceneSwitcher::UpMacroSegementHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	int actionSize = macro->Actions().size();
	int conditionSize = macro->Conditions().size();

	if (currentActionIdx == -1 && currentConditionIdx == -1) {
		if (lastInteracted == MacroSection::CONDITIONS) {
			if (conditionSize == 0) {
				MacroActionSelectionChanged(0);
			} else {
				MacroConditionSelectionChanged(0);
			}
		} else {
			if (actionSize == 0) {
				MacroConditionSelectionChanged(0);
			} else {
				MacroActionSelectionChanged(0);
			}
		}
		return;
	}

	if (currentActionIdx > 0) {
		MacroActionSelectionChanged(currentActionIdx - 1);
		return;
	}
	if (currentConditionIdx > 0) {
		MacroConditionSelectionChanged(currentConditionIdx - 1);
		return;
	}
	if (currentActionIdx == 0) {
		if (conditionSize == 0) {
			MacroActionSelectionChanged(actionSize - 1);
		} else {
			MacroConditionSelectionChanged(conditionSize - 1);
		}
		return;
	}
	if (currentConditionIdx == 0) {
		if (actionSize == 0) {
			MacroConditionSelectionChanged(conditionSize - 1);
		} else {
			MacroActionSelectionChanged(actionSize - 1);
		}
		return;
	}
}

void AdvSceneSwitcher::DownMacroSegementHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	int actionSize = macro->Actions().size();
	int conditionSize = macro->Conditions().size();

	if (currentActionIdx == -1 && currentConditionIdx == -1) {
		if (lastInteracted == MacroSection::CONDITIONS) {
			if (conditionSize == 0) {
				MacroActionSelectionChanged(0);
			} else {
				MacroConditionSelectionChanged(0);
			}
		} else {
			if (actionSize == 0) {
				MacroConditionSelectionChanged(0);
			} else {
				MacroActionSelectionChanged(0);
			}
		}
		return;
	}

	if (currentActionIdx < actionSize - 1) {
		MacroActionSelectionChanged(currentActionIdx + 1);
		return;
	}
	if (currentConditionIdx < conditionSize - 1) {
		MacroConditionSelectionChanged(currentConditionIdx + 1);
		return;
	}
	if (currentActionIdx == actionSize - 1) {
		if (conditionSize == 0) {
			MacroActionSelectionChanged(0);
		} else {
			MacroConditionSelectionChanged(0);
		}
		return;
	}
	if (currentConditionIdx == conditionSize - 1) {
		if (actionSize == 0) {
			MacroConditionSelectionChanged(0);
		} else {
			MacroActionSelectionChanged(0);
		}
		return;
	}
}

void AdvSceneSwitcher::DeleteMacroSegementHotkey()
{
	if (!MacroTabIsInFocus()) {
		return;
	}

	if (currentActionIdx != -1) {
		RemoveMacroAction(currentActionIdx);
	} else if (currentConditionIdx != -1) {
		RemoveMacroCondition(currentConditionIdx);
	}
}

static void fade(QWidget *widget, bool fadeOut)
{
	const double fadeOutOpacity = 0.3;
	// Don't use exactly 1.0 as for some reason this causes buttons in
	// macroSplitter handle layout to not be redrawn unless mousing over
	// them
	const double fadeInOpacity = 0.99;
	auto curEffect = widget->graphicsEffect();
	if (curEffect) {
		auto curOpacity =
			dynamic_cast<QGraphicsOpacityEffect *>(curEffect);
		if (curOpacity &&
		    ((fadeOut && DoubleEquals(curOpacity->opacity(),
					      fadeOutOpacity, 0.0001)) ||
		     (!fadeOut && DoubleEquals(curOpacity->opacity(),
					       fadeInOpacity, 0.0001)))) {
			return;
		}
	} else if (!fadeOut) {
		return;
	}
	delete curEffect;
	QGraphicsOpacityEffect *opacityEffect = new QGraphicsOpacityEffect();
	widget->setGraphicsEffect(opacityEffect);
	QPropertyAnimation *animation =
		new QPropertyAnimation(opacityEffect, "opacity");
	animation->setDuration(350);
	animation->setStartValue(fadeOut ? fadeInOpacity : fadeOutOpacity);
	animation->setEndValue(fadeOut ? fadeOutOpacity : fadeInOpacity);
	animation->setEasingCurve(QEasingCurve::OutQuint);
	animation->start(QPropertyAnimation::DeleteWhenStopped);
}

static void fadeWidgets(const std::vector<QWidget *> &widgets, bool fadeOut)
{
	for (const auto &widget : widgets) {
		fade(widget, fadeOut);
	}
}

void AdvSceneSwitcher::HighlightControls()
{
	const std::vector<QWidget *> conditionControls{
		ui->conditionAdd, ui->conditionRemove, ui->conditionTop,
		ui->conditionUp,  ui->conditionDown,   ui->conditionBottom,
	};
	const std::vector<QWidget *> actionControls{
		ui->actionAdd, ui->actionRemove, ui->actionTop,
		ui->actionUp,  ui->actionDown,   ui->actionBottom,
	};
	const std::vector<QWidget *> elseActionControls{
		ui->elseActionAdd, ui->elseActionRemove, ui->elseActionTop,
		ui->elseActionUp,  ui->elseActionDown,   ui->elseActionBottom,
	};

	if ((currentActionIdx == -1 && currentConditionIdx == -1 &&
	     currentElseActionIdx == -1)) {
		fadeWidgets(conditionControls, false);
		fadeWidgets(actionControls, false);
		fadeWidgets(elseActionControls, false);
	} else if (currentConditionIdx != -1) {
		fadeWidgets(conditionControls, false);
		fadeWidgets(actionControls, true);
		fadeWidgets(elseActionControls, true);
	} else if (currentActionIdx != -1) {
		fadeWidgets(conditionControls, true);
		fadeWidgets(actionControls, false);
		fadeWidgets(elseActionControls, true);
	} else if (currentElseActionIdx != -1) {
		fadeWidgets(conditionControls, true);
		fadeWidgets(actionControls, true);
		fadeWidgets(elseActionControls, false);
	} else {
		assert(false);
	}
}

} // namespace advss
