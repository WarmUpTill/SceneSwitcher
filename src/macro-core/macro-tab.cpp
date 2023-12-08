#include "macro.hpp"
#include "macro-tree.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "advanced-scene-switcher.hpp"
#include "switcher-data.hpp"
#include "name-dialog.hpp"
#include "macro-properties.hpp"
#include "macro-export-import-dialog.hpp"
#include "utility.hpp"
#include "version.h"

#include <QColor>
#include <QMenu>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace advss {

static QMetaObject::Connection addPulse;
static QTimer onChangeHighlightTimer;

static bool macroNameExists(const std::string &name)
{
	return !!GetMacroByName(name.c_str());
}

bool AdvSceneSwitcher::AddNewMacro(std::shared_ptr<Macro> &res,
				   std::string &name, std::string format)
{
	QString fmt;
	int i = 1;
	if (format.empty()) {
		fmt = {obs_module_text(
			"AdvSceneSwitcher.macroTab.defaultname")};
	} else {
		fmt = QString::fromStdString(format);
		i = 2;
	}

	QString placeHolderText = fmt.arg(i);
	while ((macroNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = fmt.arg(++i);
	}

	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.macroTab.add"),
		obs_module_text("AdvSceneSwitcher.macroTab.name"), name,
		placeHolderText);

	if (!accepted) {
		return false;
	}

	if (name.empty()) {
		return false;
	}

	if (macroNameExists(name)) {
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.macroTab.exists"));
		return false;
	}

	res = std::make_shared<Macro>(
		name, switcher->macroProperties._newMacroRegisterHotkeys);
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
	ui->macroAdd->disconnect(addPulse);
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
			"AdvSceneSwitcher.macroTab.groupDeleteConfirm");
		if (!DisplayMessage(deleteWarning.arg(name), true)) {
			return;
		}
	}

	ui->macros->Remove(macro);
	emit MacroRemoved(name);
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
	auto macros = GetSelectedMacros();
	if (macros.empty()) {
		return;
	}

	if (macros.size() == 1) {
		QString deleteWarning = obs_module_text(
			"AdvSceneSwitcher.macroTab.deleteSingleMacroConfirmation");
		if (!DisplayMessage(deleteWarning.arg(QString::fromStdString(
					    macros.at(0)->Name())),
				    true)) {
			return;
		}
		RemoveMacro(macros.at(0));
		return;
	}

	QString deleteWarning = obs_module_text(
		"AdvSceneSwitcher.macroTab.deleteMultipleMacrosConfirmation");
	if (!DisplayMessage(deleteWarning.arg(macros.size()), true)) {
		return;
	}

	for (auto &macro : macros) {
		RemoveMacro(macro);
	}
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

static bool newMacroNameValid(const std::string &name)
{
	if (!macroNameExists(name)) {
		return true;
	}
	DisplayMessage(obs_module_text("AdvSceneSwitcher.macroTab.exists"));
	return false;
}

void AdvSceneSwitcher::RenameCurrentMacro()
{
	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}
	std::string oldName = macro->Name();
	std::string name;
	if (!AdvSSNameDialog::AskForName(
		    this, obs_module_text("AdvSceneSwitcher.windowTitle"),
		    obs_module_text("AdvSceneSwitcher.item.newName"), name,
		    QString::fromStdString(oldName))) {
		return;
	}
	if (name.empty() || name == oldName || !newMacroNameValid(name)) {
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
	for (auto it = switcher->macros.begin(); it < switcher->macros.end();
	     it++) {
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

	auto data = obs_data_create();
	auto macroArray = obs_data_array_create();
	for (const auto &macro : macros) {
		obs_data_t *obj = obs_data_create();
		macro->Save(obj);
		obs_data_array_push_back(macroArray, obj);
		obs_data_release(obj);
	}
	obs_data_set_array(data, "macros", macroArray);
	switcher->SaveVariables(data);
	obs_data_array_release(macroArray);
	obs_data_set_string(data, "version", g_GIT_TAG);
	auto json = obs_data_get_json(data);
	QString exportString(json);
	obs_data_release(data);

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

	MacroSegmentList *setList, *resetList1, *resetList2;
	int *setIdx, *resetIdx1, *resetIdx2;
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
	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.macroTab.add"),
		obs_module_text("AdvSceneSwitcher.macroTab.name"), newName,
		placeHolderText);

	if (!accepted) {
		return false;
	}

	if (newName.empty()) {
		return false;
	}

	if (macroNameExists(newName)) {
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.macroTab.exists"));
		return ResolveMacroImportNameConflict(macro);
	}

	macro->SetName(newName);
	return true;
}

static bool variableWithNameExists(const std::string &name)
{
	return !!GetVariableByName(name);
}

static void importVariables(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array = obs_data_get_array(obj, "variables");
	size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease data = obs_data_array_item(array, i);
		auto var = Variable::Create();
		var->Load(data);
		if (variableWithNameExists(var->Name())) {
			continue;
		}
		switcher->variables.emplace_back(var);
	}
}

void AdvSceneSwitcher::ImportMacros()
{
	QString json;
	if (!MacroExportImportDialog::ImportMacros(json)) {
		return;
	}
	auto data = obs_data_create_from_json(json.toStdString().c_str());
	if (!data) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.macroTab.import.invalid"));
		ImportMacros();
		return;
	}
	importVariables(data);

	auto version = obs_data_get_string(data, "version");
	if (strcmp(version, g_GIT_TAG) != 0) {
		blog(LOG_WARNING,
		     "importing macros from non matching plugin version \"%s\"",
		     version);
	}

	auto array = obs_data_get_array(data, "macros");
	size_t count = obs_data_array_count(array);

	int groupSize = 0;
	std::shared_ptr<Macro> group;

	auto lock = LockContext();

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(array, i);
		auto macro = std::make_shared<Macro>();
		macro->Load(array_obj);
		macro->PostLoad();

		if (macroNameExists(macro->Name()) &&
		    !ResolveMacroImportNameConflict(macro)) {
			obs_data_release(array_obj);
			groupSize--;
			continue;
		}

		switcher->macros.emplace_back(macro);
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

		obs_data_release(array_obj);
	}
	obs_data_array_release(array);
	obs_data_release(data);

	ui->macros->Reset(switcher->macros,
			  switcher->macroProperties._highlightExecuted);
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
	    !newMacroNameValid(newName.toStdString())) {
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
}

void AdvSceneSwitcher::HighlightOnChange()
{
	if (!switcher->macroProperties._highlightActions &&
	    !switcher->macroProperties._highlightExecuted) {
		return;
	}

	auto macro = GetSelectedMacro();
	if (!macro) {
		return;
	}

	if (macro->OnChangePreventedActionsRecently()) {
		PulseWidget(ui->runMacroOnChange, Qt::yellow, Qt::transparent,
			    true);
	}
}

void AdvSceneSwitcher::on_macroProperties_clicked()
{
	MacroProperties prop = switcher->macroProperties;
	bool accepted = MacroPropertiesDialog::AskForSettings(
		this, prop, GetSelectedMacro().get());
	if (!accepted) {
		return;
	}
	switcher->macroProperties = prop;
	emit HighlightMacrosChanged(prop._highlightExecuted);
	emit HighlightActionsChanged(prop._highlightActions);
	emit HighlightConditionsChanged(prop._highlightConditions);
}

static void moveControlsToSplitter(QSplitter *splitter, int idx,
				   QLayoutItem *item)
{
	static int splitterHandleWidth = 38;
	auto handle = splitter->handle(idx);
	auto layout = item->layout();
	layout->setContentsMargins(7, 7, 7, 7);
	handle->setLayout(layout);
	splitter->setHandleWidth(splitterHandleWidth);
	splitter->setStyleSheet("QSplitter::handle {background: transparent;}");
}

bool shouldRestoreSplitter(const QList<int> &pos)
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

void AdvSceneSwitcher::SetupMacroTab()
{
	ui->macroElseActions->installEventFilter(this);

	if (switcher->macros.size() == 0 && !switcher->disableHints) {
		addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
	}
	ui->macros->Reset(switcher->macros,
			  switcher->macroProperties._highlightExecuted);
	connect(ui->macros, SIGNAL(MacroSelectionAboutToChange()), this,
		SLOT(MacroSelectionAboutToChange()));
	connect(ui->macros, SIGNAL(MacroSelectionChanged()), this,
		SLOT(MacroSelectionChanged()));
	ui->runMacro->SetMacroTree(ui->macros);

	ui->conditionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editConditionHelp"));
	connect(ui->conditionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroConditionSelectionChanged);
	connect(ui->conditionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroConditionReorder);

	ui->actionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editActionHelp"));
	connect(ui->actionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroActionSelectionChanged);
	connect(ui->actionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroActionReorder);

	ui->elseActionsList->SetHelpMsg(obs_module_text(
		"AdvSceneSwitcher.macroTab.editElseActionHelp"));
	connect(ui->elseActionsList, &MacroSegmentList::SelectionChagned, this,
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

	// Move condition controls into splitter handle layout
	moveControlsToSplitter(ui->macroActionConditionSplitter, 1,
			       ui->macroConditionsLayout->takeAt(1));
	moveControlsToSplitter(ui->macroElseActionSplitter, 1,
			       ui->macroActionsLayout->takeAt(1));

	// Set action and condition control icons
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
		&AdvSceneSwitcher::RenameCurrentMacro);
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

static void setupConextMenu(AdvSceneSwitcher *ss, const QPoint &pos,
			    std::function<void(AdvSceneSwitcher *)> expand,
			    std::function<void(AdvSceneSwitcher *)> collapse,
			    std::function<void(AdvSceneSwitcher *)> maximize,
			    std::function<void(AdvSceneSwitcher *)> minimize)
{
	QMenu menu;
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
		       ss, [ss, expand]() { expand(ss); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"),
		       ss, [ss, collapse]() { collapse(ss); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
		       ss, [ss, maximize]() { maximize(ss); });
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
		       ss, [ss, minimize]() { minimize(ss); });
	menu.exec(pos);
}

void AdvSceneSwitcher::ShowMacroActionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, ui->actionsList->mapToGlobal(pos),
			&AdvSceneSwitcher::ExpandAllActions,
			&AdvSceneSwitcher::CollapseAllActions,
			&AdvSceneSwitcher::MaximizeActions,
			&AdvSceneSwitcher::MinimizeActions);
}

void AdvSceneSwitcher::ShowMacroElseActionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, ui->elseActionsList->mapToGlobal(pos),
			&AdvSceneSwitcher::ExpandAllElseActions,
			&AdvSceneSwitcher::CollapseAllElseActions,
			&AdvSceneSwitcher::MaximizeElseActions,
			&AdvSceneSwitcher::MinimizeElseActions);
}

void AdvSceneSwitcher::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	setupConextMenu(this, ui->conditionsList->mapToGlobal(pos),
			&AdvSceneSwitcher::ExpandAllConditions,
			&AdvSceneSwitcher::CollapseAllConditions,
			&AdvSceneSwitcher::MaximizeConditions,
			&AdvSceneSwitcher::MinimizeConditions);
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

	obs_data_t *data = obs_data_create();
	macro->Save(data);
	newMacro->Load(data);
	newMacro->PostLoad();
	newMacro->SetName(name);
	Macro::PrepareMoveToGroup(macro->Parent(), newMacro);
	obs_data_release(data);

	ui->macros->Add(newMacro, macro);
	ui->macroAdd->disconnect(addPulse);
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

void AdvSceneSwitcher::FadeOutActionControls()
{
	fade(ui->actionAdd, true);
	fade(ui->actionRemove, true);
	fade(ui->actionTop, true);
	fade(ui->actionUp, true);
	fade(ui->actionDown, true);
	fade(ui->actionBottom, true);
}

void AdvSceneSwitcher::FadeOutConditionControls()
{
	fade(ui->conditionAdd, true);
	fade(ui->conditionRemove, true);
	fade(ui->conditionTop, true);
	fade(ui->conditionUp, true);
	fade(ui->conditionDown, true);
	fade(ui->conditionBottom, true);
}

void AdvSceneSwitcher::ResetOpacityActionControls()
{
	fade(ui->actionAdd, false);
	fade(ui->actionRemove, false);
	fade(ui->actionTop, false);
	fade(ui->actionUp, false);
	fade(ui->actionDown, false);
	fade(ui->actionBottom, false);
}

void AdvSceneSwitcher::ResetOpacityConditionControls()
{
	fade(ui->conditionAdd, false);
	fade(ui->conditionRemove, false);
	fade(ui->conditionTop, false);
	fade(ui->conditionUp, false);
	fade(ui->conditionDown, false);
	fade(ui->conditionBottom, false);
}

void AdvSceneSwitcher::HighlightControls()
{
	if ((currentActionIdx == -1 && currentConditionIdx == -1) ||
	    (currentActionIdx != -1 && currentConditionIdx != -1)) {
		ResetOpacityActionControls();
		ResetOpacityConditionControls();
	} else if (currentActionIdx != -1) {
		FadeOutConditionControls();
		ResetOpacityActionControls();
	} else {
		FadeOutActionControls();
		ResetOpacityConditionControls();
	}
}

} // namespace advss
