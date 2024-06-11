#include "macro-list.hpp"
#include "macro-helpers.hpp"
#include "macro-selection.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

namespace advss {

MacroList::MacroList(QWidget *parent, bool allowDuplicates, bool reorder)
	: ListEditor(parent, reorder),
	  _allowDuplicates(allowDuplicates)
{
	QWidget::connect(window(),
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));
	QWidget::connect(window(), SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	UpdateListSize();
}

void MacroList::SetContent(const std::vector<MacroRef> &macros)
{
	for (auto &m : macros) {
		QString listEntryName;
		auto macroName = m.Name();
		if (macroName.empty()) {
			listEntryName = QString::fromStdString(
				std::string("<") +
				obs_module_text(
					"AdvSceneSwitcher.macroList.deleted") +
				">");
		} else {
			listEntryName = QString::fromStdString(macroName);
		}
		QListWidgetItem *item =
			new QListWidgetItem(listEntryName, _list);
		item->setData(Qt::UserRole, listEntryName);
	}
	UpdateListSize();
}

QAction *MacroList::AddControl(QWidget *widget, bool addSeperator)
{
	if (addSeperator) {
		_controls->addSeparator();
	}
	return _controls->addWidget(widget);
}

int MacroList::CurrentRow()
{
	return _list->currentRow();
}

void MacroList::MacroRename(const QString &oldName, const QString &newName)
{
	auto count = _list->count();
	for (int idx = 0; idx < count; ++idx) {
		QListWidgetItem *item = _list->item(idx);
		QString itemString = item->data(Qt::UserRole).toString();
		if (oldName == itemString) {
			item->setData(Qt::UserRole, newName);
			item->setText(newName);
		}
	}
}

void MacroList::MacroRemove(const QString &name)
{
	int idx = FindEntry(name.toStdString());
	while (idx != -1) {
		delete _list->item(idx);
		idx = FindEntry(name.toStdString());
	}
	UpdateListSize();
}

void MacroList::Add()
{
	std::string macroName;
	bool accepted = MacroSelectionDialog::AskForMacro(this, macroName);

	if (!accepted || macroName.empty()) {
		return;
	}

	if (!_allowDuplicates && FindEntry(macroName) != -1) {
		return;
	}

	QVariant v = QVariant::fromValue(QString::fromStdString(macroName));
	auto item =
		new QListWidgetItem(QString::fromStdString(macroName), _list);
	item->setData(Qt::UserRole, QString::fromStdString(macroName));
	UpdateListSize();
	emit Added(macroName);
}

void MacroList::Remove()
{
	auto item = _list->currentItem();
	int idx = _list->currentRow();
	if (!item || idx == -1) {
		return;
	}
	delete item;
	UpdateListSize();
	emit Removed(idx);
}

void MacroList::Up()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != 0) {
		_list->insertItem(idx - 1, _list->takeItem(idx));
		_list->setCurrentRow(idx - 1);
		emit MovedUp(idx);
	}
}

void MacroList::Down()
{
	int idx = _list->currentRow();
	if (idx != -1 && idx != _list->count() - 1) {
		_list->insertItem(idx + 1, _list->takeItem(idx));
		_list->setCurrentRow(idx + 1);
		emit MovedDown(idx);
	}
}

void MacroList::Clicked(QListWidgetItem *item)
{
	std::string macroName;
	bool accepted = MacroSelectionDialog::AskForMacro(this, macroName);

	if (!accepted || macroName.empty()) {
		return;
	}

	if (!_allowDuplicates && FindEntry(macroName) != -1) {
		QString err =
			obs_module_text("AdvSceneSwitcher.macroList.duplicate");
		DisplayMessage(err.arg(QString::fromStdString(macroName)));
		return;
	}

	item->setText(QString::fromStdString(macroName));
	int idx = _list->currentRow();
	emit Replaced(idx, macroName);
}

int MacroList::FindEntry(const std::string &macro)
{
	int count = _list->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = _list->item(i);
		QString itemString = item->data(Qt::UserRole).toString();
		if (QString::fromStdString(macro) == itemString) {
			idx = i;
			break;
		}
	}

	return idx;
}

} // namespace advss
