#pragma once
#include "macro.hpp"

#include <QWidget>
#include <QDialog>
#include <QCheckBox>
#include <QGroupBox>
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

private:
	QCheckBox *_executed;
	QCheckBox *_conditions;
	QCheckBox *_actions;
	QCheckBox *_newMacroRegisterHotkeys;
	// Current macro specific settings
	QCheckBox *_currentMacroRegisterHotkeys;
	QCheckBox *_currentMacroRegisterDock;
	QCheckBox *_currentMacroDockAddRunButton;
	QCheckBox *_currentMacroDockAddPauseButton;
	QGroupBox *_dockOptions;
};

} // namespace advss
