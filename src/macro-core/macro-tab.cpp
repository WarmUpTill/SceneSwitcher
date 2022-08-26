#include "macro.hpp"
#include "macro-list-entry-widget.hpp"
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

static QMetaObject::Connection addPulse;
static QTimer onChangeHighlightTimer;

bool macroNameExists(std::string name)
{
	return !!GetMacroByName(name.c_str());
}

bool AdvSceneSwitcher::addNewMacro(std::string &name, std::string format)
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
		switcher->macros.emplace_back(
			std::make_shared<Macro>(name, true));
	}
	return true;
}

QListWidgetItem *AddNewMacroListEntry(QListWidget *list,
				      std::shared_ptr<Macro> &macro)
{
	QListWidgetItem *item = new QListWidgetItem(list);
	item->setData(Qt::UserRole, QString::fromStdString(macro->Name()));
	auto listEntry = new MacroListEntryWidget(
		macro, switcher->macroProperties._highlightExecuted, list);
	item->setSizeHint(listEntry->minimumSizeHint());
	list->setItemWidget(item, listEntry);
	return item;
}

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	std::string name;
	if (!addNewMacro(name)) {
		return;
	}
	QString text = QString::fromStdString(name);

	auto item = AddNewMacroListEntry(ui->macros, switcher->macros.back());
	ui->macros->setCurrentItem(item);

	ui->macroAdd->disconnect(addPulse);
	ui->macroHelp->setVisible(false);

	emit MacroAdded(QString::fromStdString(name));
}

void AdvSceneSwitcher::on_macroRemove_clicked()
{
	QListWidgetItem *item = ui->macros->currentItem();
	if (!item) {
		return;
	}
	int idx = ui->macros->currentRow();
	delete item;
	QString name;
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->abortMacroWait = true;
		switcher->macroWaitCv.notify_all();
		name = QString::fromStdString(switcher->macros[idx]->Name());
		switcher->macros.erase(switcher->macros.begin() + idx);
		for (auto &m : switcher->macros) {
			m->ResolveMacroRef();
		}
	}

	if (ui->macros->count() == 0) {
		ui->macroHelp->setVisible(true);
	}
	emit MacroRemoved(name);
}

void AdvSceneSwitcher::on_macroUp_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	if (!listMoveUp(ui->macros)) {
		return;
	}

	int index = ui->macros->currentRow() + 1;
	auto *entry1 = static_cast<MacroListEntryWidget *>(
		ui->macros->itemWidget(ui->macros->item(index)));
	auto *entry2 = static_cast<MacroListEntryWidget *>(
		ui->macros->itemWidget(ui->macros->item(index - 1)));
	entry1->SetMacro(*(switcher->macros.begin() + index - 1));
	entry2->SetMacro(*(switcher->macros.begin() + index));
	iter_swap(switcher->macros.begin() + index,
		  switcher->macros.begin() + index - 1);

	for (auto &m : switcher->macros) {
		m->ResolveMacroRef();
	}
}

void AdvSceneSwitcher::on_macroDown_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	if (!listMoveDown(ui->macros)) {
		return;
	}

	int index = ui->macros->currentRow() - 1;
	auto *entry1 = static_cast<MacroListEntryWidget *>(
		ui->macros->itemWidget(ui->macros->item(index)));
	auto *entry2 = static_cast<MacroListEntryWidget *>(
		ui->macros->itemWidget(ui->macros->item(index + 1)));
	entry1->SetMacro(*(switcher->macros.begin() + index + 1));
	entry2->SetMacro(*(switcher->macros.begin() + index));
	iter_swap(switcher->macros.begin() + index,
		  switcher->macros.begin() + index + 1);

	for (auto &m : switcher->macros) {
		m->ResolveMacroRef();
	}
}

void AdvSceneSwitcher::on_macroName_editingFinished()
{
	bool nameValid = true;

	Macro *macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	QString newName = ui->macroName->text();
	QString oldName = QString::fromStdString(macro->Name());

	if (newName.isEmpty() || newName == oldName) {
		nameValid = false;
	}

	if (nameValid && macroNameExists(newName.toUtf8().constData())) {
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.macroTab.exists"));
		nameValid = false;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		if (nameValid) {
			macro->SetName(newName.toUtf8().constData());
			QListWidgetItem *item = ui->macros->currentItem();
			item->setData(Qt::UserRole, newName);
			auto listEntry = static_cast<MacroListEntryWidget *>(
				ui->macros->itemWidget(item));
			listEntry->SetName(newName);
		} else {
			ui->macroName->setText(oldName);
		}
	}

	emit MacroRenamed(oldName, newName);
}

void AdvSceneSwitcher::on_runMacro_clicked()
{
	Macro *macro = getSelectedMacro();
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
	Macro *macro = getSelectedMacro();
	if (!macro) {
		return;
	}
	std::lock_guard<std::mutex> lock(switcher->m);
	macro->SetRunInParallel(value);
}

void AdvSceneSwitcher::on_runMacroOnChange_stateChanged(int value)
{
	Macro *macro = getSelectedMacro();
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

void AdvSceneSwitcher::HighlightAction(int idx)
{
	actionsList->Highlight(idx);
}

void AdvSceneSwitcher::HighlightCondition(int idx)
{
	conditionsList->Highlight(idx);
}

Macro *AdvSceneSwitcher::getSelectedMacro()
{
	QListWidgetItem *item = ui->macros->currentItem();

	if (!item) {
		return nullptr;
	}

	QString name = item->data(Qt::UserRole).toString();
	return GetMacroByQString(name);
}

void AdvSceneSwitcher::on_macros_currentRowChanged(int idx)
{
	if (loading) {
		return;
	}

	if (idx == -1) {
		SetMacroEditAreaDisabled(true);
		conditionsList->Clear();
		actionsList->Clear();
		conditionsList->SetHelpMsgVisible(true);
		actionsList->SetHelpMsgVisible(true);
		return;
	}

	QListWidgetItem *item = ui->macros->item(idx);
	QString macroName = item->data(Qt::UserRole).toString();

	auto macro = GetMacroByQString(macroName);
	if (macro) {
		SetEditMacro(*macro);
	}
}

void AdvSceneSwitcher::MacroDragDropReorder(QModelIndex, int from, int,
					    QModelIndex, int to)
{
	std::lock_guard<std::mutex> lock(switcher->m);
	if (from > to) {
		std::rotate(switcher->macros.rend() - from - 1,
			    switcher->macros.rend() - from,
			    switcher->macros.rend() - to);
	} else {
		std::rotate(switcher->macros.begin() + from,
			    switcher->macros.begin() + from + 1,
			    switcher->macros.begin() + to);
	}

	for (auto &m : switcher->macros) {
		m->ResolveMacroRef();
	}
}

void AdvSceneSwitcher::HighlightOnChange()
{
	auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	if (switcher->macroProperties._highlightExecuted &&
	    macro->OnChangePreventedActionsRecently()) {
		PulseWidget(ui->runMacroOnChange, Qt::yellow, Qt::transparent,
			    true);
	}
}

void AdvSceneSwitcher::on_macroProperties_clicked()
{
	MacroProperties prop = switcher->macroProperties;
	bool accepted = MacroPropertiesDialog::AskForSettings(
		this, prop, getSelectedMacro());
	if (!accepted) {
		return;
	}
	switcher->macroProperties = prop;
	emit HighlightMacrosChanged(prop._highlightExecuted);
	emit HighlightActionsChanged(prop._highlightActions);
	emit HighlightConditionsChanged(prop._highlightConditions);
}

void AdvSceneSwitcher::setupMacroTab()
{
	const QSignalBlocker signalBlocker(ui->macros);
	ui->macros->clear();
	for (auto &m : switcher->macros) {
		AddNewMacroListEntry(ui->macros, m);
	}

	if (switcher->macros.size() == 0) {
		if (!switcher->disableHints) {
			addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
		}
		ui->macroHelp->setVisible(true);
	} else {
		ui->macroHelp->setVisible(false);
	}

	connect(ui->macros->model(),
		SIGNAL(rowsMoved(QModelIndex, int, int, QModelIndex, int)),
		this,
		SLOT(MacroDragDropReorder(QModelIndex, int, int, QModelIndex,
					  int)));

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

	// Reserve more space for macro edit area than for the macro list
	ui->macroListMacroEditSplitter->setStretchFactor(0, 1);
	ui->macroListMacroEditSplitter->setStretchFactor(1, 4);
}

void AdvSceneSwitcher::ShowMacroContextMenu(const QPoint &pos)
{
	QPoint globalPos = ui->macros->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.copy"),
			 this, &AdvSceneSwitcher::CopyMacro);
	myMenu.exec(globalPos);
}

void AdvSceneSwitcher::ShowMacroActionsContextMenu(const QPoint &pos)
{
	QPoint globalPos = actionsList->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
			 this, &AdvSceneSwitcher::ExpandAllActions);
	myMenu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"), this,
		&AdvSceneSwitcher::CollapseAllActions);
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
			 this, &AdvSceneSwitcher::MinimizeConditions);
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
			 this, &AdvSceneSwitcher::MinimizeActions);
	myMenu.exec(globalPos);
}

void AdvSceneSwitcher::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	QPoint globalPos = conditionsList->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
			 this, &AdvSceneSwitcher::ExpandAllConditions);
	myMenu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"), this,
		&AdvSceneSwitcher::CollapseAllConditions);
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.maximize"),
			 this, &AdvSceneSwitcher::MinimizeActions);
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.minimize"),
			 this, &AdvSceneSwitcher::MinimizeConditions);
	myMenu.exec(globalPos);
}

void AdvSceneSwitcher::CopyMacro()
{
	const auto macro = getSelectedMacro();
	if (!macro) {
		return;
	}

	std::string format = macro->Name() + " %1";
	std::string name;
	if (!addNewMacro(name, format)) {
		return;
	}

	obs_data_t *data = obs_data_create();
	macro->Save(data);
	switcher->macros.back()->Load(data);
	switcher->macros.back()->SetName(name);
	obs_data_release(data);

	auto item = AddNewMacroListEntry(ui->macros, switcher->macros.back());
	ui->macros->setCurrentItem(item);
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
