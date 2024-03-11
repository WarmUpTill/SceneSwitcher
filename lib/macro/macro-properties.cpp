#include "macro-properties.hpp"
#include "layout-helpers.hpp"
#include "obs-module-helper.hpp"
#include "macro.hpp"

#include <QDialogButtonBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace advss {

static MacroProperties macroProperties;

void MacroProperties::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "highlightExecuted", _highlightExecuted);
	obs_data_set_bool(data, "highlightConditions", _highlightConditions);
	obs_data_set_bool(data, "highlightActions", _highlightActions);
	obs_data_set_bool(data, "newMacroRegisterHotkey",
			  _newMacroRegisterHotkeys);
	obs_data_set_obj(obj, "macroProperties", data);
	obs_data_release(data);
}

void MacroProperties::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "macroProperties");
	obs_data_set_default_bool(data, "highlightExecuted", true);
	obs_data_set_default_bool(data, "highlightConditions", true);
	obs_data_set_default_bool(data, "highlightActions", true);
	_highlightExecuted = obs_data_get_bool(data, "highlightExecuted");
	_highlightConditions = obs_data_get_bool(data, "highlightConditions");
	_highlightActions = obs_data_get_bool(data, "highlightActions");
	_newMacroRegisterHotkeys =
		obs_data_get_bool(data, "newMacroRegisterHotkey");
	obs_data_release(data);
}

MacroPropertiesDialog::MacroPropertiesDialog(QWidget *parent,
					     const MacroProperties &prop,
					     Macro *macro)
	: QDialog(parent),
	  _executed(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightExecutedMacros"))),
	  _conditions(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightTrueConditions"))),
	  _actions(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.highlightPerformedActions"))),
	  _newMacroRegisterHotkeys(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.newMacroRegisterHotkey"))),
	  _currentMacroRegisterHotkeys(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentDisableHotkeys"))),
	  _currentSkipOnStartup(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentSkipExecutionOnStartup"))),
	  _currentStopActionsIfNotDone(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentStopActionsIfNotDone"))),
	  _currentMacroRegisterDock(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentRegisterDock"))),
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

	auto highlightOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.highlightSettings"));
	auto highlightLayout = new QVBoxLayout;
	highlightLayout->addWidget(_executed);
	highlightLayout->addWidget(_conditions);
	highlightLayout->addWidget(_actions);
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
	generalLayout->addWidget(_currentSkipOnStartup);
	generalLayout->addWidget(_currentStopActionsIfNotDone);
	generalOptions->setLayout(generalLayout);

	int row = 0;
	_dockLayout->addWidget(_currentMacroRegisterDock, row, 1, 1, 2);
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
		&MacroPropertiesDialog::DockEnableChanged);
	connect(_currentMacroDockAddRunButton, &QCheckBox::stateChanged, this,
		&MacroPropertiesDialog::RunButtonEnableChanged);
	connect(_currentMacroDockAddPauseButton, &QCheckBox::stateChanged, this,
		&MacroPropertiesDialog::PauseButtonEnableChanged);
	connect(_currentMacroDockAddStatusLabel, &QCheckBox::stateChanged, this,
		&MacroPropertiesDialog::StatusLabelEnableChanged);

	auto scrollArea = new QScrollArea(this);
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);

	auto contentWidget = new QWidget(scrollArea);
	auto layout = new QVBoxLayout(contentWidget);
	layout->addWidget(highlightOptions);
	layout->addWidget(hotkeyOptions);
	layout->addWidget(generalOptions);
	layout->addWidget(_dockOptions);
	layout->setContentsMargins(0, 0, 0, 0);
	scrollArea->setWidget(contentWidget);

	auto dialogLayout = new QVBoxLayout();
	dialogLayout->addWidget(scrollArea);
	dialogLayout->addWidget(buttonbox);
	setLayout(dialogLayout);

	_executed->setChecked(prop._highlightExecuted);
	_conditions->setChecked(prop._highlightConditions);
	_actions->setChecked(prop._highlightActions);
	_newMacroRegisterHotkeys->setChecked(prop._newMacroRegisterHotkeys);

	if (!macro || macro->IsGroup()) {
		hotkeyOptions->hide();
		generalOptions->hide();
		_dockOptions->hide();
		return;
	}

	_currentMacroRegisterHotkeys->setChecked(macro->PauseHotkeysEnabled());
	_currentSkipOnStartup->setChecked(macro->SkipExecOnStart());
	_currentStopActionsIfNotDone->setChecked(macro->StopActionsIfNotDone());
	const bool dockEnabled = macro->DockEnabled();
	_currentMacroRegisterDock->setChecked(dockEnabled);
	_currentMacroDockAddRunButton->setChecked(macro->DockHasRunButton());
	_currentMacroDockAddPauseButton->setChecked(
		macro->DockHasPauseButton());
	_currentMacroDockAddStatusLabel->setChecked(
		macro->DockHasStatusLabel());
	_currentMacroDockHighlightIfConditionsTrue->setChecked(
		macro->DockHighlightEnabled());
	_runButtonText->setText(macro->RunButtonText());
	_pauseButtonText->setText(macro->PauseButtonText());
	_unpauseButtonText->setText(macro->UnpauseButtonText());
	_conditionsTrueStatusText->setText(macro->ConditionsTrueStatusText());
	_conditionsFalseStatusText->setText(macro->ConditionsFalseStatusText());

	_currentMacroDockAddRunButton->setVisible(dockEnabled);
	_currentMacroDockAddPauseButton->setVisible(dockEnabled);
	_currentMacroDockAddStatusLabel->setVisible(dockEnabled);
	_currentMacroDockHighlightIfConditionsTrue->setVisible(dockEnabled);
	SetGridLayoutRowVisible(_dockLayout, _runButtonTextRow,
				dockEnabled && macro->DockHasRunButton());
	SetGridLayoutRowVisible(_dockLayout, _pauseButtonTextRow,
				dockEnabled && macro->DockHasPauseButton());
	SetGridLayoutRowVisible(_dockLayout, _unpauseButtonTextRow,
				dockEnabled && macro->DockHasPauseButton());
	SetGridLayoutRowVisible(_dockLayout, _conditionsTrueTextRow,
				dockEnabled && macro->DockHasStatusLabel());
	SetGridLayoutRowVisible(_dockLayout, _conditionsFalseTextRow,
				dockEnabled && macro->DockHasStatusLabel());
	MinimizeSizeOfColumn(_dockLayout, 0);
	Resize();

	// Try to set sensible initial size for the dialog window
	QSize contentSize = contentWidget->sizeHint();
	resize(contentSize.width() + layout->contentsMargins().left() +
		       layout->contentsMargins().right() +
		       dialogLayout->contentsMargins().left() +
		       dialogLayout->contentsMargins().right() +
		       scrollArea->verticalScrollBar()->sizeHint().width() + 20,
	       contentSize.height() + dialogLayout->spacing() +
		       buttonbox->sizeHint().height() +
		       dialogLayout->contentsMargins().top() +
		       dialogLayout->contentsMargins().bottom() +
		       scrollArea->horizontalScrollBar()->sizeHint().height() +
		       20);
}

void MacroPropertiesDialog::DockEnableChanged(int enabled)
{
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

void MacroPropertiesDialog::RunButtonEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _runButtonTextRow, enabled);
	Resize();
}

void MacroPropertiesDialog::PauseButtonEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _pauseButtonTextRow, enabled);
	SetGridLayoutRowVisible(_dockLayout, _unpauseButtonTextRow, enabled);
	Resize();
}

void MacroPropertiesDialog::StatusLabelEnableChanged(int enabled)
{
	SetGridLayoutRowVisible(_dockLayout, _conditionsTrueTextRow, enabled);
	SetGridLayoutRowVisible(_dockLayout, _conditionsFalseTextRow, enabled);
	Resize();
}

void MacroPropertiesDialog::Resize()
{
	_dockOptions->adjustSize();
	_dockOptions->updateGeometry();
}

bool MacroPropertiesDialog::AskForSettings(QWidget *parent,
					   MacroProperties &userInput,
					   Macro *macro)
{
	MacroPropertiesDialog dialog(parent, userInput, macro);
	dialog.setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	if (dialog.exec() != DialogCode::Accepted) {
		return false;
	}
	userInput._highlightExecuted = dialog._executed->isChecked();
	userInput._highlightConditions = dialog._conditions->isChecked();
	userInput._highlightActions = dialog._actions->isChecked();
	userInput._newMacroRegisterHotkeys =
		dialog._newMacroRegisterHotkeys->isChecked();
	if (!macro) {
		return true;
	}

	macro->EnablePauseHotkeys(
		dialog._currentMacroRegisterHotkeys->isChecked());
	macro->SetSkipExecOnStart(dialog._currentSkipOnStartup->isChecked());
	macro->SetStopActionsIfNotDone(
		dialog._currentStopActionsIfNotDone->isChecked());
	macro->EnableDock(dialog._currentMacroRegisterDock->isChecked());
	macro->SetDockHasRunButton(
		dialog._currentMacroDockAddRunButton->isChecked());
	macro->SetDockHasPauseButton(
		dialog._currentMacroDockAddPauseButton->isChecked());
	macro->SetDockHasStatusLabel(
		dialog._currentMacroDockAddStatusLabel->isChecked());
	macro->SetHighlightEnable(
		dialog._currentMacroDockHighlightIfConditionsTrue->isChecked());
	macro->SetRunButtonText(dialog._runButtonText->text().toStdString());
	macro->SetPauseButtonText(
		dialog._pauseButtonText->text().toStdString());
	macro->SetUnpauseButtonText(
		dialog._unpauseButtonText->text().toStdString());
	macro->SetConditionsTrueStatusText(
		dialog._conditionsTrueStatusText->text().toStdString());
	macro->SetConditionsFalseStatusText(
		dialog._conditionsFalseStatusText->text().toStdString());

	return true;
}

MacroProperties &GetGlobalMacroProperties()
{
	return macroProperties;
}

void SaveGlobalMacroProperties(obs_data_t *obj)
{
	macroProperties.Save(obj);
}

void LoadGlobalMacroProperties(obs_data_t *obj)
{
	macroProperties.Load(obj);
}

} // namespace advss
