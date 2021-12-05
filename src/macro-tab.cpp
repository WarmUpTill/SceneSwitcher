#include "headers/macro.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/name-dialog.hpp"
#include "headers/utility.hpp"

#include <QColor>
#include <QMenu>

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

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	std::string name;
	if (!addNewMacro(name)) {
		return;
	}
	QString text = QString::fromStdString(name);

	QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
	item->setData(Qt::UserRole, text);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setCheckState(Qt::Checked);
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
	int index = ui->macros->currentRow();
	if (index != -1 && index != 0) {
		ui->macros->insertItem(index - 1, ui->macros->takeItem(index));
		ui->macros->setCurrentRow(index - 1);

		iter_swap(switcher->macros.begin() + index,
			  switcher->macros.begin() + index - 1);

		for (auto &m : switcher->macros) {
			m->ResolveMacroRef();
		}
	}
}

void AdvSceneSwitcher::on_macroDown_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	int index = ui->macros->currentRow();
	if (index != -1 && index != ui->macros->count() - 1) {
		ui->macros->insertItem(index + 1, ui->macros->takeItem(index));
		ui->macros->setCurrentRow(index + 1);

		iter_swap(switcher->macros.begin() + index,
			  switcher->macros.begin() + index + 1);

		for (auto &m : switcher->macros) {
			m->ResolveMacroRef();
		}
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
			// Don't trigger itemChanged()
			// pause state remains as is
			ui->macros->blockSignals(true);
			item->setText(newName);
			ui->macros->blockSignals(false);
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

	bool ret = macro->PerformAction(true);
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

void AdvSceneSwitcher::PopulateMacroActions(Macro &m, uint32_t afterIdx)
{
	auto &actions = m.Actions();
	for (; afterIdx < actions.size(); afterIdx++) {
		auto newEntry = new MacroActionEdit(this, &actions[afterIdx],
						    actions[afterIdx]->GetId());
		ConnectControlSignals(newEntry);
		ui->macroEditActionLayout->addWidget(newEntry);
		ui->macroEditActionHelp->setVisible(false);
	}
	ui->macroEditActionHelp->setVisible(actions.size() == 0);
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
		ui->macroEditConditionLayout->addWidget(newEntry);
		ui->macroEditConditionHelp->setVisible(false);
		root = false;
	}
	ui->macroEditConditionHelp->setVisible(conditions.size() == 0);
}

void AdvSceneSwitcher::SetEditMacro(Macro &m)
{
	ui->macroName->setText(m.Name().c_str());
	ui->runMacroInParallel->setChecked(m.RunInParallel());
	clearLayout(ui->macroEditConditionLayout);
	clearLayout(ui->macroEditActionLayout);

	PopulateMacroConditions(m);
	PopulateMacroActions(m);
	ui->macroEdit->setDisabled(false);
}

void AdvSceneSwitcher::HighlightAction(int idx)
{
	auto item = ui->macroEditActionLayout->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, QColor(Qt::green), QColor(0, 0, 0, 0), "QLabel ",
		    true);
}

void AdvSceneSwitcher::HighlightCondition(int idx)
{
	auto item = ui->macroEditConditionLayout->itemAt(idx);
	if (!item) {
		return;
	}
	auto widget = item->widget();
	if (!widget) {
		return;
	}
	PulseWidget(widget, QColor(Qt::green), QColor(0, 0, 0, 0), "QLabel ",
		    true);
}

void AdvSceneSwitcher::ConnectControlSignals(MacroActionEdit *c)
{
	connect(c, &MacroActionEdit::AddAt, this,
		&AdvSceneSwitcher::AddMacroAction);
	connect(c, &MacroActionEdit::RemoveAt, this,
		&AdvSceneSwitcher::RemoveMacroAction);
	connect(c, &MacroActionEdit::UpAt, this,
		&AdvSceneSwitcher::MoveMacroActionUp);
	connect(c, &MacroActionEdit::DownAt, this,
		&AdvSceneSwitcher::MoveMacroActionDown);
}

void AdvSceneSwitcher::ConnectControlSignals(MacroConditionEdit *c)
{
	connect(c, &MacroConditionEdit::AddAt, this,
		&AdvSceneSwitcher::AddMacroCondition);
	connect(c, &MacroConditionEdit::RemoveAt, this,
		&AdvSceneSwitcher::RemoveMacroCondition);
	connect(c, &MacroConditionEdit::UpAt, this,
		&AdvSceneSwitcher::MoveMacroConditionUp);
	connect(c, &MacroConditionEdit::DownAt, this,
		&AdvSceneSwitcher::MoveMacroConditionDown);
}

Macro *AdvSceneSwitcher::getSelectedMacro()
{
	QListWidgetItem *item = ui->macros->currentItem();

	if (!item) {
		return nullptr;
	}

	QString name = item->text();
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
	QString macroName = item->text();

	auto macro = GetMacroByQString(macroName);
	if (macro) {
		SetEditMacro(*macro);
	}
}

void AdvSceneSwitcher::on_macros_itemChanged(QListWidgetItem *item)
{
	if (loading) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	QString name = item->text();

	auto m = GetMacroByQString(name);
	if (m) {
		m->SetPaused(item->checkState() != Qt::Checked);
	}
}

void AdvSceneSwitcher::setupMacroTab()
{
	for (auto &m : switcher->macros) {
		QString text = QString::fromStdString(m->Name());

		QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
		item->setData(Qt::UserRole, text);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		if (m->Paused()) {
			item->setCheckState(Qt::Unchecked);
		} else {
			item->setCheckState(Qt::Checked);
		}
	}

	if (switcher->macros.size() == 0) {
		if (!switcher->disableHints) {
			addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
		}
		ui->macroHelp->setVisible(true);
	} else {
		ui->macroHelp->setVisible(false);
	}

	ui->macros->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macros, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroContextMenu);
	ui->macroActions->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macroActions, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroActionsContextMenu);
	ui->macroConditions->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macroConditions, &QWidget::customContextMenuRequested, this,
		&AdvSceneSwitcher::ShowMacroConditionsContextMenu);

	ui->macroEdit->setDisabled(true);

	ui->macroPriorityWarning->setVisible(
		switcher->functionNamesByPriority[0] != macro_func);
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
	QPoint globalPos = ui->macroActions->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
			 this, &AdvSceneSwitcher::ExpandAllActions);
	myMenu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"), this,
		&AdvSceneSwitcher::CollapseAllActions);
	myMenu.exec(globalPos);
}

void AdvSceneSwitcher::ShowMacroConditionsContextMenu(const QPoint &pos)
{
	QPoint globalPos = ui->macroConditions->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.expandAll"),
			 this, &AdvSceneSwitcher::ExpandAllConditions);
	myMenu.addAction(
		obs_module_text("AdvSceneSwitcher.macroTab.collapseAll"), this,
		&AdvSceneSwitcher::CollapseAllConditions);
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

	QString text = QString::fromStdString(name);
	QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
	item->setData(Qt::UserRole, text);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setCheckState(Qt::Checked);
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
	setCollapsedStateOfSegmentsIn(ui->macroEditActionLayout, false);
}

void AdvSceneSwitcher::ExpandAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(ui->macroEditConditionLayout, false);
}

void AdvSceneSwitcher::CollapseAllActions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(ui->macroEditActionLayout, true);
}

void AdvSceneSwitcher::CollapseAllConditions()
{
	auto m = getSelectedMacro();
	if (!m) {
		return;
	}
	setCollapsedStateOfSegmentsIn(ui->macroEditConditionLayout, true);
}
