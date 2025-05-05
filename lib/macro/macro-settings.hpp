#pragma once
#include "variable-line-edit.hpp"
#include "duration-control.hpp"
#include "macro-input.hpp"

#include <QWidget>
#include <QDialog>
#include <QCheckBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QGridLayout>
#include <obs-data.h>

namespace advss {

class Macro;

class GlobalMacroSettings {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);

	bool _highlightExecuted = true;
	bool _highlightConditions = true;
	bool _highlightActions = true;
	bool _newMacroCheckInParallel = false;
	bool _newMacroRegisterHotkeys = true;
	bool _newMacroUseShortCircuitEvaluation = false;
	bool _saveSettingsOnMacroChange = true;
};

// Dialog for configuring global and individual macro specific settings
class MacroSettingsDialog : public QDialog {
	Q_OBJECT

public:
	MacroSettingsDialog(QWidget *parent, const GlobalMacroSettings &,
			    Macro *macro);
	static bool AskForSettings(QWidget *parent,
				   GlobalMacroSettings &userInput,
				   Macro *macro);
private slots:
	void DockEnableChanged(int);
	void RunButtonEnableChanged(int);
	void PauseButtonEnableChanged(int);
	void StatusLabelEnableChanged(int);

private:
	void Resize();
	void SetCustomConditionIntervalWarningVisibility() const;

	// Global macro settings
	QCheckBox *_highlightExecutedMacros;
	QCheckBox *_highlightConditions;
	QCheckBox *_highlightActions;
	QCheckBox *_newMacroCheckInParallel;
	QCheckBox *_newMacroRegisterHotkeys;
	QCheckBox *_newMacroUseShortCircuitEvaluation;
	QCheckBox *_saveSettingsOnMacroChange;
	// Current macro specific settings
	QCheckBox *_currentCheckInParallel;
	QCheckBox *_currentMacroRegisterHotkeys;
	QCheckBox *_currentUseShortCircuitEvaluation;
	QCheckBox *_currentUseCustomConditionCheckInterval;
	DurationSelection *_currentCustomConditionCheckInterval;
	QLabel *_currentCustomConditionCheckIntervalWarning;
	QComboBox *_currentPauseSaveBehavior;
	QCheckBox *_currentSkipOnStartup;
	QCheckBox *_currentStopActionsIfNotDone;
	MacroInputSelection *_currentInputs;
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

GlobalMacroSettings &GetGlobalMacroSettings();
void SaveGlobalMacroSettings(obs_data_t *obj);
void LoadGlobalMacroSettings(obs_data_t *obj);

} // namespace advss
