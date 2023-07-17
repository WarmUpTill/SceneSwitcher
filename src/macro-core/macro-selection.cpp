#include "macro-selection.hpp"
#include "advanced-scene-switcher.hpp"
#include "switcher-data.hpp"
#include "utility.hpp"

#include <QStandardItemModel>
#include <QDialogButtonBox>

namespace advss {

MacroSelection::MacroSelection(QWidget *parent)
	: FilterComboBox(parent,
			 obs_module_text("AdvSceneSwitcher.selectMacro"))
{
	for (const auto &m : switcher->macros) {
		if (m->IsGroup()) {
			continue;
		}
		addItem(QString::fromStdString(m->Name()));
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

void MacroSelection::SetCurrentMacro(const MacroRef &macro)
{
	auto m = macro.GetMacro();
	if (!m) {
		this->setCurrentIndex(-1);
	} else {
		this->setCurrentText(QString::fromStdString(m->Name()));
	}
}

void MacroSelection::HideSelectedMacro()
{
	auto ssWindow = static_cast<AdvSceneSwitcher *>(window());
	if (!ssWindow) {
		return;
	}

	const auto m = ssWindow->ui->macros->GetCurrentMacro();
	if (!m) {
		return;
	}
	int idx = findText(QString::fromStdString(m->Name()));
	if (idx == -1) {
		return;
	}

	qobject_cast<QListView *>(view())->setRowHidden(idx, true);
}

void MacroSelection::ShowAllMacros()
{
	auto v = qobject_cast<QListView *>(view());
	for (int i = count(); i > 0; i--) {
		v->setRowHidden(i, false);
	}
}

void MacroSelection::MacroRemove(const QString &name)
{
	int idx = findText(name);
	if (idx == -1) {
		return;
	}
	removeItem(idx);
	setCurrentIndex(-1);
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

MacroSelectionDialog::MacroSelectionDialog(QWidget *)
{
	setModal(true);
	setWindowModality(Qt::WindowModality::ApplicationModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setMinimumWidth(350);
	setMinimumHeight(70);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	_macroSelection = new MacroSelection(window());
	auto *selectionLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{macroSelection}}", _macroSelection},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.askForMacro"),
		     selectionLayout, widgetPlaceholders);
	auto *layout = new QVBoxLayout();
	layout->addLayout(selectionLayout);
	layout->addWidget(buttonbox);
	setLayout(layout);
}

bool MacroSelectionDialog::AskForMacro(QWidget *parent, std::string &macroName)
{
	MacroSelectionDialog dialog(parent);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));

	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	macroName = dialog._macroSelection->currentText().toStdString();
	if (macroName == obs_module_text("AdvSceneSwitcher.selectMacro") ||
	    macroName.empty()) {
		return false;
	}

	return true;
}

} // namespace advss
