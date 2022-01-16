#pragma once
#include <QComboBox>

class Macro;

class MacroSelection : public QComboBox {
	Q_OBJECT

public:
	MacroSelection(QWidget *parent);
	void SetCurrentMacro(Macro *);
	void HideSelectedMacro(); // Macro currently being edited
	void ShowAllMacros();

private slots:
	void MacroAdd(const QString &name);
	void MacroRemove(const QString &name);
	void MacroRename(const QString &oldName, const QString &newName);
};
