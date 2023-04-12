#include "macro.hpp"
#include "macro-tree.hpp"
#include "macro-action-edit.hpp"
#include "macro-condition-edit.hpp"
#include "advanced-scene-switcher.hpp"
#include "name-dialog.hpp"
#include "macro-properties.hpp"
#include "utility.hpp"

#include <QColor>
#include <QMenu>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

namespace advss {

static QMetaObject::Connection addPulse;
static QTimer onChangeHighlightTimer;

bool macroNameExists(std::string name)
{
	return !!GetMacroByName(name.c_str());
}

bool AdvSceneSwitcher::addNewMacro(std::shared_ptr<Macro> &res,
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

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		res = std::make_shared<Macro>(
			name,
			switcher->macroProperties._newMacroRegisterHotkeys);
	}
	return true;
}

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	std::string name;
	std::shared_ptr<Macro> newMacro;
	if (!addNewMacro(newMacro, name)) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		ui->macros->Add(newMacro);
	}

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

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		ui->macros->Remove(macro);
	}

	emit MacroRemoved(name);
}

void AdvSceneSwitcher::RenameMacro(std::shared_ptr<Macro> &macro,
				   const QString &name)
{
	auto oldName = QString::fromStdString(macro->Name());
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		macro->SetName(name.toStdString());
	}
	emit MacroRenamed(oldName, name);
}

void AdvSceneSwitcher::on_macroRemove_clicked()
{
	auto macros = getSelectedMacros();
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
	std::lock_guard<std::mutex> lock(switcher->m);
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	ui->macros->Up(macro);
}

void AdvSceneSwitcher::on_macroDown_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	auto macro = getSelectedMacro();
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
	auto macro = getSelectedMacro();
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

void AdvSceneSwitcher::on_macroName_editingFinished()
{
	auto macro = getSelectedMacro();
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

void AdvSceneSwitcher::on_runMacro_clicked()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	bool ret = macro->PerformActions(true, true);
	if (!ret) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroTab.runFail");
		DisplayMessage(err.arg(QString::fromStdString(macro->Name())));
	}
}

void AdvSceneSwitcher::on_runMacroInParallel_stateChanged(int value)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	macro->SetRunInParallel(value);
}

void AdvSceneSwitcher::on_runMacroOnChange_stateChanged(int value)
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	macro->SetMatchOnChange(value);
}

void AdvSceneSwitcher::PopulateMacroActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.Actions();
	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		actionsList->Add(newEntry);
	}
	actionsList->SetHelpMsgVisible(actions.size() == 0);
}

void AdvSceneSwitcher::PopulateMacroConditions(Macro &m, uint32_t afterIdx)
{
	bool root = afterIdx == 0;
	auto &conditions = m.Conditions();
	for (; afterIdx < conditions.size(); afterIdx++) {
		auto newEntry = new MacroConditionEdit(
			this, &conditions[afterIdx],
			conditions[afterIdx]->GetId(), root);
		conditionsList->Add(newEntry);
		root = false;
	}
	conditionsList->SetHelpMsgVisible(conditions.size() == 0);
}

void AdvSceneSwitcher::SetActionData(Macro &m)
{
	auto &actions = m.Actions();
	for (int idx = 0; idx < actionsList->ContentLayout()->count(); idx++) {
		auto item = actionsList->ContentLayout()->itemAt(idx);
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
	for (int idx = 0; idx < conditionsList->ContentLayout()->count();
	     idx++) {
		auto item = conditionsList->ContentLayout()->itemAt(idx);
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
	conditionsList->Clear();
	actionsList->Clear();

	m.ResetUIHelpers();

	PopulateMacroConditions(m);
	PopulateMacroActions(m);
	SetMacroEditAreaDisabled(false);
	if (m.IsGroup()) {
		SetMacroEditAreaDisabled(true);
		ui->macroName->setEnabled(true);
	}

	currentActionIdx = -1;
	currentConditionIdx = -1;
	HighlightControls();
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
	actionsList->Highlight(idx, color);
}

void AdvSceneSwitcher::HighlightCondition(int idx, QColor color)
{
	conditionsList->Highlight(idx, color);
}

std::shared_ptr<Macro> AdvSceneSwitcher::getSelectedMacro()
{
	return ui->macros->GetCurrentMacro();
}

std::vector<std::shared_ptr<Macro>> AdvSceneSwitcher::getSelectedMacros()
{
	return ui->macros->GetCurrentMacros();
}

void AdvSceneSwitcher::MacroSelectionChanged(const QItemSelection &,
					     const QItemSelection &)
{
	if (loading) {
		return;
	}

	auto macro = getSelectedMacro();
	if (!macro) {
		SetMacroEditAreaDisabled(true);
		conditionsList->Clear();
		actionsList->Clear();
		conditionsList->SetHelpMsgVisible(true);
		actionsList->SetHelpMsgVisible(true);
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

	auto macro = getSelectedMacro();
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
		this, prop, getSelectedMacro().get());
	if (!accepted) {
		return;
	}
	switcher->macroProperties = prop;
	emit HighlightMacrosChanged(prop._highlightExecuted);
	emit HighlightActionsChanged(prop._highlightActions);
	emit HighlightConditionsChanged(prop._highlightConditions);
}

// Don't restore splitter pos if an element is not visible at all
bool shouldResotreSplitterPos(const QList<int> &pos)
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

void AdvSceneSwitcher::setupMacroTab()
{
	if (switcher->macros.size() == 0 && !switcher->disableHints) {
		addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
	}
	ui->macros->Reset(switcher->macros,
			  switcher->macroProperties._highlightExecuted);
	connect(ui->macros->selectionModel(),
		SIGNAL(selectionChanged(const QItemSelection &,
					const QItemSelection &)),
		this,
		SLOT(MacroSelectionChanged(const QItemSelection &,
					   const QItemSelection &)));

	delete conditionsList;
	conditionsList = new MacroSegmentList(this);
	conditionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editConditionHelp"));
	connect(conditionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroConditionSelectionChanged);
	connect(conditionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroConditionReorder);
	ui->macroConditionsLayout->insertWidget(0, conditionsList);

	delete actionsList;
	actionsList = new MacroSegmentList(this);
	actionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editActionHelp"));
	connect(actionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroActionSelectionChanged);
	connect(actionsList, &MacroSegmentList::Reorder, this,
		&AdvSceneSwitcher::MacroActionReorder);
	ui->macroActionsLayout->insertWidget(0, actionsList);

	ui->macros->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macros, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroContextMenu);
	actionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(actionsList, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroActionsContextMenu);
	conditionsList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(conditionsList, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroConditionsContextMenu);

	SetMacroEditAreaDisabled(true);
	ui->macroPriorityWarning->setVisible(
		switcher->functionNamesByPriority[0] != macro_func);

	onChangeHighlightTimer.setInterval(1500);
	connect(&onChangeHighlightTimer, SIGNAL(timeout()), this,
		SLOT(HighlightOnChange()));
	onChangeHighlightTimer.start();

	// Move condition controls into splitter handle layout
	auto handle = ui->macroActionConditionSplitter->handle(1);
	auto item = ui->macroConditionsLayout->takeAt(1);
	if (item) {
		auto layout = item->layout();
		layout->setContentsMargins(7, 7, 7, 7);
		handle->setLayout(layout);
		ui->macroActionConditionSplitter->setHandleWidth(38);
	}
	ui->macroActionConditionSplitter->setStyleSheet(
		"QSplitter::handle {background: transparent;}");

	// Reserve more space for macro edit area than for the macro list
	ui->macroListMacroEditSplitter->setStretchFactor(0, 1);
	ui->macroListMacroEditSplitter->setStretchFactor(1, 4);

	if (switcher->saveWindowGeo) {
		if (shouldResotreSplitterPos(
			    switcher->macroActionConditionSplitterPosition)) {
			ui->macroActionConditionSplitter->setSizes(
				switcher->macroActionConditionSplitterPosition);
		}
		if (shouldResotreSplitterPos(
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

	menu.exec(globalPos);
}

void AdvSceneSwitcher::ShowMacroActionsContextMenu(const QPoint &pos)
{
	QPoint globalPos = actionsList->mapToGlobal(pos);
	QMenu menu;
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
		       this, &AdvSceneSwitcher::ExpandAllActions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"),
		       this, &AdvSceneSwitcher::CollapseAllActions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
		       this, &AdvSceneSwitcher::MinimizeConditions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
		       this, &AdvSceneSwitcher::MinimizeActions);
	menu.exec(globalPos);
}

void AdvSceneSwitcher::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	QPoint globalPos = conditionsList->mapToGlobal(pos);
	QMenu menu;
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
		       this, &AdvSceneSwitcher::ExpandAllConditions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"),
		       this, &AdvSceneSwitcher::CollapseAllConditions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
		       this, &AdvSceneSwitcher::MinimizeActions);
	menu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
		       this, &AdvSceneSwitcher::MinimizeConditions);
	menu.exec(globalPos);
}

void AdvSceneSwitcher::CopyMacro()
{
	const auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	std::string format = macro->Name() + " %1";
	std::string name;
	std::shared_ptr<Macro> newMacro;
	if (!addNewMacro(newMacro, name, format)) {
		return;
	}

	obs_data_t *data = obs_data_create();
	macro->Save(data);
	newMacro->Load(data);
	newMacro->PostLoad();
	newMacro->SetName(name);
	obs_data_release(data);

	ui->macros->Add(newMacro);
	ui->macroAdd->disconnect(addPulse);
	emit MacroAdded(QString::fromStdString(name));
}

void AdvSceneSwitcher::ExpandAllActions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	actionsList->SetCollapsed(false);
}

void AdvSceneSwitcher::ExpandAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	conditionsList->SetCollapsed(false);
}

void AdvSceneSwitcher::CollapseAllActions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	actionsList->SetCollapsed(true);
}

void AdvSceneSwitcher::CollapseAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	conditionsList->SetCollapsed(true);
}

void AdvSceneSwitcher::MinimizeActions()
{
	QList<int> sizes = ui->macroActionConditionSplitter->sizes();
	int sum = sizes[0] + sizes[1];
	int actionsHeight = sum / 10;
	sizes[1] = actionsHeight;
	sizes[0] = sum - actionsHeight;
	ui->macroActionConditionSplitter->setSizes(sizes);
}

void AdvSceneSwitcher::MinimizeConditions()
{
	QList<int> sizes = ui->macroActionConditionSplitter->sizes();
	int sum = sizes[0] + sizes[1];
	int conditionsHeight = sum / 10;
	sizes[0] = conditionsHeight;
	sizes[1] = sum - conditionsHeight;
	ui->macroActionConditionSplitter->setSizes(sizes);
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

	auto macro = getSelectedMacro();
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

	auto macro = getSelectedMacro();
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

void fade(QWidget *widget, bool fadeOut)
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
		    ((fadeOut && doubleEquals(curOpacity->opacity(),
					      fadeOutOpacity, 0.0001)) ||
		     (!fadeOut && doubleEquals(curOpacity->opacity(),
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
	fade(ui->actionUp, true);
	fade(ui->actionDown, true);
}

void AdvSceneSwitcher::FadeOutConditionControls()
{
	fade(ui->conditionAdd, true);
	fade(ui->conditionRemove, true);
	fade(ui->conditionUp, true);
	fade(ui->conditionDown, true);
}

void AdvSceneSwitcher::ResetOpacityActionControls()
{
	fade(ui->actionAdd, false);
	fade(ui->actionRemove, false);
	fade(ui->actionUp, false);
	fade(ui->actionDown, false);
}

void AdvSceneSwitcher::ResetOpacityConditionControls()
{
	fade(ui->conditionAdd, false);
	fade(ui->conditionRemove, false);
	fade(ui->conditionUp, false);
	fade(ui->conditionDown, false);
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
