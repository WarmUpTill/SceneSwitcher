#include "headers/macro.hpp"
#include "headers/macro-list-entry-widget.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/name-dialog.hpp"
#include "headers/utility.hpp"

#include <QColor>
#include <QMenu>

static QTimer highlightMatchTimer;
static QMetaObject::Connection addPulse;

bool macroNameExists(std::string name)
{
	return !!GetMacroByName(name.c_str());
}

bool AdvSceneSwitcher::addNewMacro(std::string &name)
{
	QString format{
		obs_module_text("AdvSceneSwitcher.macroTab.defaultname")};

	int i = 1;
	QString placeHolderText = format.arg(i);
	while ((macroNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = format.arg(++i);
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
		switcher->macros.emplace_back(std::make_shared<Macro>(name));
	}
	return true;
}

QListWidgetItem *AddNewMacroListEntry(QListWidget *list,
				      std::shared_ptr<Macro> &macro)
{
	QListWidgetItem *item = new QListWidgetItem(list);
	item->setData(Qt::UserRole, QString::fromStdString(macro->Name()));
	auto listEntry = new MacroListEntryWidget(macro, list);
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
	QString name;
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->abortMacroWait = true;
		switcher->macroWaitCv.notify_all();
		int idx = ui->macros->currentRow();
		QString::fromStdString(switcher->macros[idx]->Name());
		switcher->macros.erase(switcher->macros.begin() + idx);
		for (auto &m : switcher->macros) {
			m->ResolveMacroRef();
		}
	}

	delete item;

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
		ConnectControlSignals(newEntry);
		actionsList->ContentLayout()->addWidget(newEntry);
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
		ConnectControlSignals(newEntry);
		conditionsList->ContentLayout()->addWidget(newEntry);
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
	clearLayout(conditionsList->ContentLayout());
	clearLayout(actionsList->ContentLayout());

	PopulateMacroConditions(m);
	PopulateMacroActions(m);
	ui->macroEdit->setDisabled(false);

	currentActionIdx = -1;
	currentConditionIdx = -1;
}

void AdvSceneSwitcher::HighlightAction(int idx)
{
	auto item = actionsList->ContentLayout()->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, QColor(Qt::green), QColor(0, 0, 0, 0), true);
}

void AdvSceneSwitcher::HighlightCondition(int idx)
{
	auto item = conditionsList->ContentLayout()->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, QColor(Qt::green), QColor(0, 0, 0, 0), true);
}

void AdvSceneSwitcher::ConnectControlSignals(MacroActionEdit *a)
{
	connect(a, &MacroActionEdit::SelectionChagned, this,
		&AdvSceneSwitcher::MacroActionSelectionChanged);
}

void AdvSceneSwitcher::ConnectControlSignals(MacroConditionEdit *c)
{
	connect(c, &MacroActionEdit::SelectionChagned, this,
		&AdvSceneSwitcher::MacroConditionSelectionChanged);
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
		ui->macroEdit->setDisabled(true);
		return;
	}

	QListWidgetItem *item = ui->macros->item(idx);
	QString macroName = item->data(Qt::UserRole).toString();

	auto macro = GetMacroByQString(macroName);
	if (macro) {
		SetEditMacro(*macro);
	}
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

	delete conditionsList;
	conditionsList = new MacroSegmentList(this);
	conditionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editConditionHelp"));
	connect(conditionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroConditionSelectionChanged);
	ui->macroConditionsLayout->insertWidget(0, conditionsList);

	delete actionsList;
	actionsList = new MacroSegmentList(this);
	actionsList->SetHelpMsg(
		obs_module_text("AdvSceneSwitcher.macroTab.editActionHelp"));
	connect(actionsList, &MacroSegmentList::SelectionChagned, this,
		&AdvSceneSwitcher::MacroActionSelectionChanged);
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

	ui->macroEdit->setDisabled(true);

	ui->macroPriorityWarning->setVisible(
		switcher->functionNamesByPriority[0] != macro_func);

	highlightMatchTimer.setInterval(1000);
	connect(&highlightMatchTimer, &QTimer::timeout, this,
		&AdvSceneSwitcher::HighlightMatchedMacros);
	highlightMatchTimer.start();
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
	obs_data_t *data = obs_data_create();
	getSelectedMacro()->Save(data);

	std::string name;
	if (!addNewMacro(name)) {
		obs_data_release(data);
		return;
	}

	switcher->macros.back()->Load(data);
	switcher->macros.back()->SetName(name);
	obs_data_release(data);

	auto item = AddNewMacroListEntry(ui->macros, switcher->macros.back());
	ui->macros->setCurrentItem(item);
}

void setCollapsedStateOfSegmentsIn(QLayout *layout, bool collapse)
{
	QLayoutItem *item = nullptr;
	for (int i = 0; i < layout->count(); i++) {
		item = layout->itemAt(i);
		auto segment = dynamic_cast<MacroSegmentEdit *>(item->widget());
		if (segment) {
			segment->SetCollapsed(collapse);
		}
	}
}

void AdvSceneSwitcher::ExpandAllActions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(actionsList->ContentLayout(), false);
}

void AdvSceneSwitcher::ExpandAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(conditionsList->ContentLayout(), false);
}

void AdvSceneSwitcher::CollapseAllActions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(actionsList->ContentLayout(), true);
}

void AdvSceneSwitcher::CollapseAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(conditionsList->ContentLayout(), true);
}

void AdvSceneSwitcher::MinimizeActions()
{
	QList<int> sizes = ui->macroSplitter->sizes();
	int sum = sizes[0] + sizes[1];
	int actionsHeight = sum / 10;
	sizes[1] = actionsHeight;
	sizes[0] = sum - actionsHeight;
	ui->macroSplitter->setSizes(sizes);
}

void AdvSceneSwitcher::MinimizeConditions()
{
	QList<int> sizes = ui->macroSplitter->sizes();
	int sum = sizes[0] + sizes[1];
	int conditionsHeight = sum / 10;
	sizes[0] = conditionsHeight;
	sizes[1] = sum - conditionsHeight;
	ui->macroSplitter->setSizes(sizes);
}

void AdvSceneSwitcher::HighlightMatchedMacros()
{
	if (loading || !(switcher && switcher->highlightExecutedMacros)) {
		return;
	}

	for (int idx = 0; idx < (int)switcher->macros.size(); idx++) {
		if (switcher->macros[idx]->WasExecutedRecently()) {
			auto item = ui->macros->item(idx);
			if (!item) {
				continue;
			}
			auto widget = ui->macros->itemWidget(item);
			if (!widget) {
				continue;
			}
			PulseWidget(widget, Qt::green, QColor(0, 0, 0, 0),
				    true);
		}
	}
}
