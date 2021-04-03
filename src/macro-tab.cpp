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

void AdvSceneSwitcher::on_macroUp_clicked() {}

void AdvSceneSwitcher::on_macroDown_clicked() {}

void AdvSceneSwitcher::on_macroName_editingFinished() {}

void AdvSceneSwitcher::SetEditMacro(Macro &m)
{
	ui->macroName->setText(m.Name().c_str());
	clearLayout(ui->macroEditConditionLayout);
	clearLayout(ui->macroEditActionLayout);

	for (auto &c : m.Conditions()) {
		//add conditions widgets
	}

	for (auto &a : m.Actions()) {
		//add action widgets
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
	MacroConditionEdit *test = new MacroConditionEdit();
	ui->macroEditConditionLayout->addWidget(test);

	auto *test2 = new MacroActionEdit();
	ui->macroEditActionLayout->addWidget(test2);
}
