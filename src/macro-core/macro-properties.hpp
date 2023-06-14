#pragma once
#include "macro.hpp"
#include "variable-line-edit.hpp"

#include <QWidget>
#include <QDialog>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QGridLayout>
#include <obs-data.h>

namespace advss {

class MacroProperties {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	bool _highlightExecuted = false;
	bool _highlightConditions = false;
	bool _highlightActions = false;
	bool _newMacroRegisterHotkeys = true;
};

// Dialog for configuring global and individual macro specific settings
class MacroPropertiesDialog : public QDialog {
	Q_OBJECT

public:
	MacroPropertiesDialog(QWidget *parent, const MacroProperties &,
			      Macro *macro);
	static bool AskForSettings(QWidget *parent, MacroProperties &userInput,
				   Macro *macro);
private slots:
	void DockEnableChanged(int);
	void RunButtonEnableChanged(int);
	void PauseButtonEnableChanged(int);
	void StatusLabelEnableChanged(int);

private:
	void Resize();

	QCheckBox *_executed;
	QCheckBox *_conditions;
	QCheckBox *_actions;
	QCheckBox *_newMacroRegisterHotkeys;
	// Current macro specific settings
	QCheckBox *_currentMacroRegisterHotkeys;
	QCheckBox *_currentMacroRegisterDock;
	QCheckBox *_currentMacroDockAddRunButton;
	QCheckBox *_currentMacroDockAddPauseButton;
	QCheckBox *_currentMacroDockAddStatusLabel;
	QCheckBox *_currentMacroDockHighlightIfConditionsTrue;
	VariableLineEdit *_runButtonText;
	VariableLineEdit *_pauseButtonText;
	VariableLineEdit *_unpauseButtonText;
	VariableLineEdit *_conditionsTrueStatusText;
	VariableLineEdit *_conditionsFalseStatusText;
	QGroupBox *_dockOptions;
	QGridLayout *_dockLayout;

	int _runButtonTextRow = -1;
	int _pauseButtonTextRow = -1;
	int _unpauseButtonTextRow = -1;
	int _conditionsTrueTextRow = -1;
	int _conditionsFalseTextRow = -1;
};

} // namespace advss
