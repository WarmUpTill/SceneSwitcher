#include "headers/macro.hpp"
#include "headers/macro-action-edit.hpp"
#include "headers/macro-condition-edit.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/name-dialog.hpp"

#include <QMenu>

static QMetaObject::Connection addPulse;

const auto conditionsCollapseThreshold = 4;
const auto actionsCollapseThreshold = 2;

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
		switcher->macros.emplace_back(name);
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
		int idx = ui->macros->currentRow();
		QString::fromStdString(switcher->macros[idx].Name());
		switcher->macros.erase(switcher->macros.begin() + idx);
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

void AdvSceneSwitcher::SetEditMacro(Macro &m)
{
	ui->macroName->setText(m.Name().c_str());
	clearLayout(ui->macroEditConditionLayout);
	clearLayout(ui->macroEditActionLayout);

	bool collapse = m.Conditions().size() > conditionsCollapseThreshold;
	bool root = true;
	for (auto &c : m.Conditions()) {
		auto newEntry = new MacroConditionEdit(this, &c, c->GetId(),
						       root, collapse);
		ui->macroEditConditionLayout->addWidget(newEntry);
		ui->macroEditConditionHelp->setVisible(false);
		root = false;
	}

	collapse = m.Actions().size() > actionsCollapseThreshold;
	for (auto &a : m.Actions()) {
		auto newEntry =
			new MacroActionEdit(this, &a, a->GetId(), collapse);
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
		QString text = QString::fromStdString(m.Name());

		QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
		item->setData(Qt::UserRole, text);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		if (m.Paused()) {
			item->setCheckState(Qt::Unchecked);
		} else {
			item->setCheckState(Qt::Checked);
		}
	}

	if (switcher->macros.size() == 0) {
		addPulse = PulseWidget(ui->macroAdd, QColor(Qt::green));
		ui->macroHelp->setVisible(true);
	} else {
		ui->macroHelp->setVisible(false);
	}

	ui->macros->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->macros, SIGNAL(customContextMenuRequested(QPoint)), this,
		SLOT(showMacroContextMenu(QPoint)));

	ui->macroEdit->setDisabled(true);
}

void AdvSceneSwitcher::showMacroContextMenu(const QPoint &pos)
{
	QPoint globalPos = ui->macros->mapToGlobal(pos);
	QMenu myMenu;
	myMenu.addAction(obs_module_text("AdvSceneSwitcher.macroTab.copy"),
			 this, SLOT(copyMacro()));
	myMenu.exec(globalPos);
}

void AdvSceneSwitcher::copyMacro()
{
	obs_data_t *data = obs_data_create();
	getSelectedMacro()->Save(data);

	std::string name;
	if (!addNewMacro(name)) {
		obs_data_release(data);
		return;
	}

	switcher->macros.back().Load(data);
	switcher->macros.back().SetName(name);
	obs_data_release(data);

	QString text = QString::fromStdString(name);
	QListWidgetItem *item = new QListWidgetItem(text, ui->macros);
	item->setData(Qt::UserRole, text);
	item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
	item->setCheckState(Qt::Checked);
	ui->macros->setCurrentItem(item);
}
