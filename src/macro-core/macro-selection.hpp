#pragma once
#include "filter-combo-box.hpp"

#include <QDialog>

namespace advss {

class Macro;
class MacroRef;

class MacroSelection : public FilterComboBox {
	Q_OBJECT

public:
	MacroSelection(QWidget *parent);
	void SetCurrentMacro(const MacroRef &);
	void HideSelectedMacro(); // Macro currently being edited
	void ShowAllMacros();

private slots:
	void MacroAdd(const QString &name);
	void MacroRemove(const QString &name);
	void MacroRename(const QString &oldName, const QString &newName);
};

class MacroSelectionDialog : public QDialog {
	Q_OBJECT

public:
	MacroSelectionDialog(QWidget *parent);
	static bool AskForMacro(QWidget *parent, std::string &macroName);

private:
	MacroSelection *_macroSelection;
};

} // namespace advss
