#include "headers/macro-selection.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <QStandardItemModel>

MacroSelection::MacroSelection(QWidget *parent) : QComboBox(parent)
{
	addItem(obs_module_text("AdvSceneSwitcher.selectMacro"));

	QStandardItemModel *model =
		qobject_cast<QStandardItemModel *>(this->model());
	QModelIndex firstIndex =
		model->index(0, modelColumn(), rootModelIndex());
	QStandardItem *firstItem = model->itemFromIndex(firstIndex);
	firstItem->setSelectable(false);
	firstItem->setEnabled(false);

	for (auto &m : switcher->macros) {
		addItem(QString::fromStdString(m.Name()));
	}

	QWidget::connect(parent, SIGNAL(MacroAdded(const QString &)), this,
			 SLOT(MacroAdd(const QString &)));
	QWidget::connect(parent, SIGNAL(MacroRemoved(const QString &)), this,
			 SLOT(MacroRemove(const QString &)));
	QWidget::connect(parent,
			 SIGNAL(MacroRenamed(const QString &, const QString &)),
			 this,
			 SLOT(MacroRename(const QString &, const QString &)));
}

void MacroSelection::MacroAdd(const QString &name)
{
	addItem(name);
}

void MacroSelection::SetCurrentMacro(Macro *m)
{
	if (!m) {
		this->setCurrentIndex(0);
	} else {
		this->setCurrentText(QString::fromStdString(m->Name()));
	}
}

void MacroSelection::MacroRemove(const QString &name)
{
	int idx = findText(name);
	if (idx == -1) {
		return;
	}
	removeItem(idx);
	setCurrentIndex(0);
}

void MacroSelection::MacroRename(const QString &oldName, const QString &newName)
{
	bool renameSelected = currentText() == oldName;
	int idx = findText(oldName);
	if (idx == -1) {
		return;
	}
	removeItem(idx);
	insertItem(idx, newName);
	if (renameSelected) {
		setCurrentIndex(findText(newName));
	}
}
