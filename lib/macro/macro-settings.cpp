#include "macro-settings.hpp"
#include "layout-helpers.hpp"
#include "macro.hpp"
#include "obs-module-helper.hpp"
#include "plugin-state-helpers.hpp"

#include <QDialogButtonBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace advss {

static GlobalMacroSettings macroSettings;

void GlobalMacroSettings::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "highlightExecuted", _highlightExecuted);
	obs_data_set_bool(data, "highlightConditions", _highlightConditions);
	obs_data_set_bool(data, "highlightActions", _highlightActions);
	obs_data_set_bool(data, "newMacroCheckInParallel",
			  _newMacroCheckInParallel);
	obs_data_set_bool(data, "newMacroRegisterHotkey",
			  _newMacroRegisterHotkeys);
	obs_data_set_bool(data, "newMacroUseShortCircuitEvaluation",
			  _newMacroUseShortCircuitEvaluation);
	obs_data_set_bool(data, "saveSettingsOnMacroChange",
			  _saveSettingsOnMacroChange);
	obs_data_set_obj(obj, "macroSettings", data);
	obs_data_release(data);
}

void GlobalMacroSettings::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(
		obj, obs_data_has_user_value(obj, "macroProperties")
			     ? "macroProperties"
			     : "macroSettings");
	obs_data_set_default_bool(data, "highlightExecuted", true);
	obs_data_set_default_bool(data, "highlightConditions", true);
	obs_data_set_default_bool(data, "highlightActions", true);
	_highlightExecuted = obs_data_get_bool(data, "highlightExecuted");
	_highlightConditions = obs_data_get_bool(data, "highlightConditions");
	_highlightActions = obs_data_get_bool(data, "highlightActions");
	_newMacroCheckInParallel =
		obs_data_get_bool(data, "newMacroCheckInParallel");
	_newMacroRegisterHotkeys =
		obs_data_get_bool(data, "newMacroRegisterHotkey");
	_newMacroUseShortCircuitEvaluation =
		obs_data_get_bool(data, "newMacroUseShortCircuitEvaluation");
	_saveSettingsOnMacroChange =
		obs_data_has_user_value(data, "saveSettingsOnMacroChange")
			? obs_data_get_bool(data, "saveSettingsOnMacroChange")
			: true;
	obs_data_release(data);
}

MacroSettingsDialog::MacroSettingsDialog(QWidget *parent,
					 const GlobalMacroSettings &settings,
					 Macro *macro)
	: QDialog(parent),
	  _highlightExecutedMacros(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightExecutedMacros"))),
	  _highlightConditions(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightTrueConditions"))),
	  _highlightActions(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightPerformedActions"))),
	  _newMacroCheckInParallel(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.newMacroCheckInParallel"))),
	  _newMacroRegisterHotkeys(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.newMacroRegisterHotkey"))),
	  _newMacroUseShortCircuitEvaluation(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.newMacroUseShortCircuitEvaluation"))),
	  _saveSettingsOnMacroChange(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.saveSettingsOnMacroChange"))),
	  _currentCheckInParallel(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentCheckInParallel"))),
	  _currentMacroRegisterHotkeys(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentRegisterHotkeys"))),
	  _currentUseShortCircuitEvaluation(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentUseShortCircuitEvaluation"))),
	  _currentUseCustomConditionCheckInterval(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentUseCustomConditionCheckInterval"))),
	  _currentCustomConditionCheckInterval(
		  new DurationSelection(this, true, 0.01)),
	  _currentCustomConditionCheckIntervalWarning(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentUseCustomConditionCheckIntervalWarning"))),
	  _currentPauseSaveBehavior(new QComboBox(this)),
	  _currentSkipOnStartup(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentSkipExecutionOnStartup"))),
	  _currentStopActionsIfNotDone(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentStopActionsIfNotDone"))),
	  _currentInputs(new MacroInputSelection()),
	  _currentMacroRegisterDock(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentRegisterDock"))),
	  _currentMacroIsStandaloneDock(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentIsStandaloneDock"))),
	  _currentMacroDockWindowName(new QLineEdit(this)),
	  _currentMacroDockAddRunButton(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentDockAddRunButton"))),
	  _currentMacroDockAddPauseButton(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentDockAddPauseButton"))),
	  _currentMacroDockAddStatusLabel(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentDockAddStatusLabel"))),
	  _currentMacroDockHighlightIfConditionsTrue(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentDockHighlightIfExecuted"))),
	  _runButtonText(new VariableLineEdit(this)),
	  _pauseButtonText(new VariableLineEdit(this)),
	  _unpauseButtonText(new VariableLineEdit(this)),
	  _conditionsTrueStatusText(new VariableLineEdit(this)),
	  _conditionsFalseStatusText(new VariableLineEdit(this)),
	  _dockOptions(new QGroupBox(
		  obs_module_text("AdvSceneSwitcher.macroTab.dockSettings"))),
	  _dockLayout(new QGridLayout())
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	_newMacroCheckInParallel->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.checkInParallel.tooltip"));
	_currentCheckInParallel->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.checkInParallel.tooltip"));

	_newMacroUseShortCircuitEvaluation->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.shortCircuit.tooltip"));
	_saveSettingsOnMacroChange->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.saveSettingsOnMacroChange.tooltip"));
	_currentUseShortCircuitEvaluation->setToolTip(obs_module_text(
		"AdvSceneSwitcher.macroTab.shortCircuit.tooltip"));

	_currentPauseSaveBehavior->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.pauseStateSaveBehavior.persist"),
		static_cast<int>(Macro::PauseStateSaveBehavior::PERSIST));
	_currentPauseSaveBehavior->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.pauseStateSaveBehavior.pause"),
		static_cast<int>(Macro::PauseStateSaveBehavior::PAUSE));
	_currentPauseSaveBehavior->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.pauseStateSaveBehavior.unpause"),
		static_cast<int>(Macro::PauseStateSaveBehavior::UNPAUSE));

	auto highlightOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.highlightSettings"));
	auto highlightLayout = new QVBoxLayout;
	highlightLayout->addWidget(_highlightExecutedMacros);
	highlightLayout->addWidget(_highlightConditions);
	highlightLayout->addWidget(_highlightActions);
	highlightOptions->setLayout(highlightLayout);

	auto hotkeyOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.hotkeySettings"));
	auto hotkeyLayout = new QVBoxLayout;
	hotkeyLayout->addWidget(_newMacroRegisterHotkeys);
	hotkeyLayout->addWidget(_currentMacroRegisterHotkeys);
	hotkeyOptions->setLayout(hotkeyLayout);

	auto generalOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.generalSettings"));
	auto generalLayout = new QVBoxLayout;
	generalLayout->addWidget(_saveSettingsOnMacroChange);
	generalLayout->addWidget(_currentCheckInParallel);
	generalLayout->addWidget(_newMacroCheckInParallel);
	generalLayout->addWidget(_currentSkipOnStartup);
	generalLayout->addWidget(_currentStopActionsIfNotDone);
	generalLayout->addWidget(_currentUseShortCircuitEvaluation);
	generalLayout->addWidget(_newMacroUseShortCircuitEvaluation);

	auto durationLayout = new QHBoxLayout();
	durationLayout->addWidget(_currentUseCustomConditionCheckInterval);
	durationLayout->addWidget(_currentCustomConditionCheckInterval);
	durationLayout->addStretch();
	auto customConditionIntervalLayout = new QVBoxLayout();
	customConditionIntervalLayout->addLayout(durationLayout);
	customConditionIntervalLayout->addWidget(
		_currentCustomConditionCheckIntervalWarning);
	generalLayout->addLayout(customConditionIntervalLayout);

	auto pauseStateSaveBehavorLayout = new QHBoxLayout();
	pauseStateSaveBehavorLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.pauseStateSaveBehavior")));
	pauseStateSaveBehavorLayout->addWidget(_currentPauseSaveBehavior);
	pauseStateSaveBehavorLayout->addStretch();
	generalLayout->addLayout(pauseStateSaveBehavorLayout);

	generalOptions->setLayout(generalLayout);

	auto inputOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.inputSettings"));
	auto description = new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.inputSettings.description"));
	description->setWordWrap(true);
	auto inputLayout = new QVBoxLayout();
	inputLayout->addWidget(description);
	inputLayout->addWidget(_currentInputs);
	inputOptions->setLayout(inputLayout);

	int row = 0;
	_dockLayout->addWidget(_currentMacroRegisterDock, row, 1, 1, 2);
	row++;
	_dockLayout->addWidget(_currentMacroIsStandaloneDock, row, 1, 1, 2);
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockWindowName")),
		row, 1);
	_dockLayout->addWidget(_currentMacroDockWindowName, row, 2);
	_dockWindowNameRow = row;
	row++;
	_dockLayout->addWidget(_currentMacroDockAddRunButton, row, 1, 1, 2);
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockButtonText.run")),
		row, 1);
	_dockLayout->addWidget(_runButtonText, row, 2);
	_runButtonTextRow = row;
	row++;
	_dockLayout->addWidget(_currentMacroDockAddPauseButton, row, 1, 1, 2);
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockButtonText.pause")),
		row, 1);
	_dockLayout->addWidget(_pauseButtonText, row, 2);
	_pauseButtonTextRow = row;
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockButtonText.unpause")),
		row, 1);
	_dockLayout->addWidget(_unpauseButtonText, row, 2);
	_unpauseButtonTextRow = row;
	row++;
	_dockLayout->addWidget(_currentMacroDockAddStatusLabel, row, 1, 1, 2);
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockStatusText.true")),
		row, 1);
	_dockLayout->addWidget(_conditionsTrueStatusText, row, 2);
	_conditionsTrueTextRow = row;
	row++;
	_dockLayout->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.currentDockStatusText.false")),
		row, 1);
	_dockLayout->addWidget(_conditionsFalseStatusText, row, 2);
	_conditionsFalseTextRow = row;
	row++;
	_dockLayout->addWidget(_currentMacroDockHighlightIfConditionsTrue, row,
			       1, 1, 2);

	_dockOptions->setLayout(_dockLayout);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	connect(_currentMacroRegisterDock, &QCheckBox::stateChanged, this,
		&MacroSettingsDialog::DockEnableChanged);
	connect(_currentMacroIsStandaloneDock, &QCheckBox::stateChanged, this,
		&MacroSettingsDialog::IsStandaloneDockChanged);
	connect(_currentMacroDockAddRunButton, &QCheckBox::stateChanged, this,
		&MacroSettingsDialog::RunButtonEnableChanged);
	connect(_currentMacroDockAddPauseButton, &QCheckBox::stateChanged, this,
		&MacroSettingsDialog::PauseButtonEnableChanged);
	connect(_currentMacroDockAddStatusLabel, &QCheckBox::stateChanged, this,
		&MacroSettingsDialog::StatusLabelEnableChanged);
	connect(_currentUseCustomConditionCheckInterval,
		&QCheckBox::stateChanged, this, [this](int state) {
			_currentCustomConditionCheckInterval->setEnabled(state);
		});
	connect(_currentCustomConditionCheckInterval,
		&DurationSelection::DurationChanged, this,
		[this]() { SetCustomConditionIntervalWarningVisibility(); });

	auto scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	auto contentWidget = new QWidget(scrollArea);
	auto layout = new QVBoxLayout(contentWidget);
	layout->addWidget(highlightOptions);
	layout->addWidget(hotkeyOptions);
	layout->addWidget(generalOptions);
	layout->addWidget(inputOptions);
	layout->addWidget(_dockOptions);
	layout->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(contentWidget);

	auto dialogLayout = new QVBoxLayout();
	dialogLayout->addWidget(scrollArea);
	dialogLayout->addWidget(buttonbox);
	setLayout(dialogLayout);

	_highlightExecutedMacros->setChecked(settings._highlightExecuted);
	_highlightConditions->setChecked(settings._highlightConditions);
	_highlightActions->setChecked(settings._highlightActions);
	_newMacroCheckInParallel->setChecked(settings._newMacroCheckInParallel);
	_newMacroRegisterHotkeys->setChecked(settings._newMacroRegisterHotkeys);
	_newMacroUseShortCircuitEvaluation->setChecked(
		settings._newMacroUseShortCircuitEvaluation);
	_saveSettingsOnMacroChange->setChecked(
		settings._saveSettingsOnMacroChange);

	if (!macro || macro->IsGroup()) {
		// General group
		_currentSkipOnStartup->hide();
		_currentStopActionsIfNotDone->hide();
		_currentUseShortCircuitEvaluation->hide();
		_currentCheckInParallel->hide();
		SetLayoutVisible(customConditionIntervalLayout, false);
		SetLayoutVisible(pauseStateSaveBehavorLayout, false);

		// Hotkey group
		_currentMacroRegisterHotkeys->hide();

		inputOptions->hide();
		_dockOptions->hide();
		return;
	}

	_currentCheckInParallel->setChecked(macro->CheckInParallel());
	_currentMacroRegisterHotkeys->setChecked(macro->PauseHotkeysEnabled());
	_currentUseShortCircuitEvaluation->setChecked(
		macro->ShortCircuitEvaluationEnabled());
	_currentUseCustomConditionCheckInterval->setChecked(
		macro->CustomConditionCheckIntervalEnabled());
	_currentCustomConditionCheckInterval->SetDuration(
		macro->GetCustomConditionCheckInterval());
	_currentCustomConditionCheckInterval->setEnabled(
		macro->CustomConditionCheckIntervalEnabled());
	SetCustomConditionIntervalWarningVisibility();
	_currentPauseSaveBehavior->setCurrentIndex(
		_currentPauseSaveBehavior->findData(
			static_cast<int>(macro->GetPauseStateSaveBehavior())));
	_currentSkipOnStartup->setChecked(macro->SkipExecOnStart());
	_currentStopActionsIfNotDone->setChecked(macro->StopActionsIfNotDone());
	_currentInputs->SetInputs(macro->GetInputVariables());
	const auto &dockSettings = macro->GetDockSettings();
	const bool dockEnabled = dockSettings.DockEnabled();
	_currentMacroRegisterDock->setChecked(dockEnabled);
	_currentMacroIsStandaloneDock->setChecked(
		dockSettings.IsStandaloneDock());
	_currentMacroDockWindowName->setText(
		QString::fromStdString(dockSettings.DockWindowName()));
	_currentMacroDockAddRunButton->setChecked(dockSettings.HasRunButton());
	_currentMacroDockAddPauseButton->setChecked(
		dockSettings.HasPauseButton());
	_currentMacroDockAddStatusLabel->setChecked(
		dockSettings.HasStatusLabel());
	_currentMacroDockHighlightIfConditionsTrue->setChecked(
		dockSettings.HighlightEnabled());
	_runButtonText->setText(dockSettings.RunButtonText());
	_pauseButtonText->setText(dockSettings.PauseButtonText());
	_unpauseButtonText->setText(dockSettings.UnpauseButtonText());
	_conditionsTrueStatusText->setText(
		dockSettings.ConditionsTrueStatusText());
	_conditionsFalseStatusText->setText(
		dockSettings.ConditionsFalseStatusText());

	DockEnableChanged(dockEnabled);
	MinimizeSizeOfColumn(_dockLayout, 0);

	Resize();
}

void MacroSettingsDialog::DockEnableChanged(int enabled)
{
	_currentMacroIsStandaloneDock->setVisible(enabled);
	_currentMacroDockAddRunButton->setVisible(enabled);
	_currentMacroDockAddPauseButton->setVisible(enabled);
	_currentMacroDockAddStatusLabel->setVisible(enabled);
	_currentMacroDockHighlightIfConditionsTrue->setVisible(enabled);
	SetGridLayoutRowVisible(
		_dockLayout, _runButtonTextRow,
		enabled && _currentMacroDockAddRunButton->isChecked());
	SetGridLayoutRowVisible(
		_dockLayout, _pauseButtonTextRow,
		enabled && _currentMacroDockAddPauseButton->isChecked());
	SetGridLayoutRowVisible(
		_dockLayout, _unpauseButtonTextRow,
		enabled && _currentMacroDockAddPauseButton->isChecked());
	SetGridLayoutRowVisible(
		_dockLayout, _conditionsTrueTextRow,
		enabled && _currentMacroDockAddStatusLabel->isChecked());
	SetGridLayoutRowVisible(
		_dockLayout, _conditionsFalseTextRow,
		enabled && _currentMacroDockAddStatusLabel->isChecked());

	Resize();
}

void MacroSettingsDialog::IsStandaloneDockChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _dockWindowNameRow, !enabled);
	Resize();
}

void MacroSettingsDialog::RunButtonEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _runButtonTextRow, enabled);
	Resize();
}

void MacroSettingsDialog::PauseButtonEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _pauseButtonTextRow, enabled);
	SetGridLayoutRowVisible(_dockLayout, _unpauseButtonTextRow, enabled);
	Resize();
}

void MacroSettingsDialog::StatusLabelEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _conditionsTrueTextRow, enabled);
	SetGridLayoutRowVisible(_dockLayout, _conditionsFalseTextRow, enabled);
	Resize();
}

void MacroSettingsDialog::Resize()
{
	_dockOptions->adjustSize();
	_dockOptions->updateGeometry();
}

void MacroSettingsDialog::SetCustomConditionIntervalWarningVisibility() const
{
	_currentCustomConditionCheckIntervalWarning->setVisible(
		_currentUseCustomConditionCheckInterval->isChecked() &&
		GetIntervalValue() >
			_currentCustomConditionCheckInterval->GetDuration()
				.Milliseconds());
}

bool MacroSettingsDialog::AskForSettings(QWidget *parent,
					 GlobalMacroSettings &userInput,
					 Macro *macro)
{
	MacroSettingsDialog dialog(parent, userInput, macro);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userInput._highlightExecuted =
		dialog._highlightExecutedMacros->isChecked();
	userInput._highlightConditions =
		dialog._highlightConditions->isChecked();
	userInput._highlightActions = dialog._highlightActions->isChecked();
	userInput._newMacroCheckInParallel =
		dialog._newMacroCheckInParallel->isChecked();
	userInput._newMacroRegisterHotkeys =
		dialog._newMacroRegisterHotkeys->isChecked();
	userInput._newMacroUseShortCircuitEvaluation =
		dialog._newMacroUseShortCircuitEvaluation->isChecked();
	userInput._saveSettingsOnMacroChange =
		dialog._saveSettingsOnMacroChange->isChecked();
	if (!macro) {
		return true;
	}

	macro->SetCheckInParallel(dialog._currentCheckInParallel->isChecked());
	macro->EnablePauseHotkeys(
		dialog._currentMacroRegisterHotkeys->isChecked());
	macro->SetShortCircuitEvaluation(
		dialog._currentUseShortCircuitEvaluation->isChecked());
	macro->SetCustomConditionCheckIntervalEnabled(
		dialog._currentUseCustomConditionCheckInterval->isChecked());
	macro->SetCustomConditionCheckInterval(
		dialog._currentCustomConditionCheckInterval->GetDuration());
	macro->SetPauseStateSaveBehavior(
		static_cast<Macro::PauseStateSaveBehavior>(
			dialog._currentPauseSaveBehavior->currentData()
				.toInt()));
	macro->SetSkipExecOnStart(dialog._currentSkipOnStartup->isChecked());
	macro->SetStopActionsIfNotDone(
		dialog._currentStopActionsIfNotDone->isChecked());

	auto &dockSettings = macro->GetDockSettings();
	dockSettings.EnableDock(dialog._currentMacroRegisterDock->isChecked());
	dockSettings.SetIsStandaloneDock(
		dialog._currentMacroIsStandaloneDock->isChecked());
	dockSettings.SetDockWindowName(
		dialog._currentMacroDockWindowName->text().toStdString());
	dockSettings.SetHasRunButton(
		dialog._currentMacroDockAddRunButton->isChecked());
	dockSettings.SetHasPauseButton(
		dialog._currentMacroDockAddPauseButton->isChecked());
	dockSettings.SetHasStatusLabel(
		dialog._currentMacroDockAddStatusLabel->isChecked());
	dockSettings.SetHighlightEnable(
		dialog._currentMacroDockHighlightIfConditionsTrue->isChecked());
	dockSettings.SetRunButtonText(
		dialog._runButtonText->text().toStdString());
	dockSettings.SetPauseButtonText(
		dialog._pauseButtonText->text().toStdString());
	dockSettings.SetUnpauseButtonText(
		dialog._unpauseButtonText->text().toStdString());
	dockSettings.SetConditionsTrueStatusText(
		dialog._conditionsTrueStatusText->text().toStdString());
	dockSettings.SetConditionsFalseStatusText(
		dialog._conditionsFalseStatusText->text().toStdString());
	macro->SetInputVariables(dialog._currentInputs->GetInputs());
	return true;
}

GlobalMacroSettings &GetGlobalMacroSettings()
{
	return macroSettings;
}

void SaveGlobalMacroSettings(obs_data_t *obj)
{
	macroSettings.Save(obj);
}

void LoadGlobalMacroSettings(obs_data_t *obj)
{
	macroSettings.Load(obj);
}

} // namespace advss
