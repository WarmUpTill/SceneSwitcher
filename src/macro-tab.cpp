#include "headers/macro-entry.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/name-dialog.hpp"

static QMetaObject::Connection addPulse;

bool macroNameExists(std::string name)
{
	for (auto &m : switcher->macros) {
		if (m.Name() == name) {
			return true;
		}
	}

	return false;
}

void AdvSceneSwitcher::on_macroAdd_clicked()
{
	std::string name;
	QString format{
		obs_module_text("AdvSceneSwitcher.macroTab.defaultname")};

	int i = 1;
	QString placeHolderText = format.arg(i);
	while ((macroNameExists(placeHolderText.toUtf8().constData()))) {
		placeHolderText = format.arg(++i);
	}

	bool accepted = AdvSSNameDialog::AskForName(
		this, obs_module_text("AdvSceneSwitcher.macroTab.add"),
		obs_module_text("AdvSceneSwitcher.macroTab.add"), name,
		placeHolderText);

	if (!accepted) {
		return;
	}

	if (name.empty()) {
		return;
	}

	if (macroNameExists(name)) {
		DisplayMessage(
			obs_module_text("AdvSceneSwitcher.macroTab.exists"));
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->macros.emplace_back(name);
	}
	QString text = QString::fromStdString(name);

	QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
	item->setData(Qt::UserRole, text);
	ui->macros->setCurrentItem(item);

	ui->macroAdd->disconnect(addPulse);
	ui->macroHelp->setVisible(false);
}

void AdvSceneSwitcher::on_macroRemove_clicked()
{
	QListWidgetItem *item = ui->macros->currentItem();
	if (!item) {
		return;
	}
	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->macros->currentRow();
		switcher->macros.erase(switcher->macros.begin() + idx);
	}

	delete item;

	if (ui->macros->count() == 0) {
		ui->macroHelp->setVisible(true);
	}
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
			item->setText(newName);
		} else {
			ui->macroName->setText(oldName);
		}
	}
}

void AdvSceneSwitcher::SetEditMacro(Macro &m)
{
	ui->macroName->setText(m.Name().c_str());
	clearLayout(ui->macroEditConditionLayout);
	clearLayout(ui->macroEditActionLayout);

	bool root = true;
	for (auto &c : m.Conditions()) {
		auto newEntry = new MacroConditionEdit(&c, c->GetId(), root);
		ui->macroEditConditionLayout->addWidget(newEntry);
		ui->macroEditConditionHelp->setVisible(false);
		root = false;
	}

	for (auto &a : m.Actions()) {
		auto newEntry = new MacroActionEdit(&a, a->GetId());
		ui->macroEditActionLayout->addWidget(newEntry);
		ui->macroEditActionHelp->setVisible(false);
	}

	ui->macroEdit->setDisabled(false);

	if (m.Conditions().size() == 0) {
		ui->macroEditConditionHelp->setVisible(true);
	} else {
		ui->macroEditConditionHelp->setVisible(false);
	}

	if (m.Actions().size() == 0) {
		ui->macroEditActionHelp->setVisible(true);
	} else {
		ui->macroEditActionHelp->setVisible(false);
	}
}

Macro *AdvSceneSwitcher::getSelectedMacro()
{
	Macro *macro = nullptr;
	QListWidgetItem *item = ui->macros->currentItem();

	if (!item) {
		return macro;
	}

	QString name = item->data(Qt::UserRole).toString();
	for (auto &m : switcher->macros) {
		if (name.compare(m.Name().c_str()) == 0) {
			macro = &m;
			break;
		}
	}

	return macro;
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

	for (auto &m : switcher->macros) {
		if (macroName.compare(m.Name().c_str()) == 0) {
			SetEditMacro(m);
			break;
		}
	}
}

void AdvSceneSwitcher::setupMacroTab()
{
	for (auto &m : switcher->macros) {
		QString text = QString::fromStdString(m.Name());

		QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
		item->setData(Qt::UserRole, text);
	}

	if (switcher->macros.size() == 0) {
		addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
		ui->macroHelp->setVisible(true);
	} else {
		ui->macroHelp->setVisible(false);
	}
}
