#include "macro-properties.hpp"

#include <QVBoxLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <obs-module.h>

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
	  _currentMacroRegisterDock(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.currentRegisterDock")))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setMinimumWidth(500);
	setMinimumHeight(300);

	auto highlightOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.highlightSettings"));
	QVBoxLayout *highlightLayout = new QVBoxLayout;
	highlightLayout->addWidget(_executed);
	highlightLayout->addWidget(_conditions);
	highlightLayout->addWidget(_actions);
	highlightOptions->setLayout(highlightLayout);

	auto hotkeyOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.hotkeySettings"));
	QVBoxLayout *hotkeyLayout = new QVBoxLayout;
	hotkeyLayout->addWidget(_newMacroRegisterHotkeys);
	hotkeyLayout->addWidget(_currentMacroRegisterHotkeys);
	hotkeyOptions->setLayout(hotkeyLayout);

	auto dockOptions = new QGroupBox(
		obs_module_text("AdvSceneSwitcher.macroTab.dockSettings"));
	QVBoxLayout *dockLayout = new QVBoxLayout;
	dockLayout->addWidget(_currentMacroRegisterDock);
	dockOptions->setLayout(dockLayout);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(highlightOptions);
	layout->addWidget(hotkeyOptions);
	layout->addWidget(dockOptions);
	layout->addWidget(buttonbox);
	setLayout(layout);

	_executed->setChecked(prop._highlightExecuted);
	_conditions->setChecked(prop._highlightConditions);
	_actions->setChecked(prop._highlightActions);
	_newMacroRegisterHotkeys->setChecked(prop._newMacroRegisterHotkeys);
	if (macro && !macro->IsGroup()) {
		_currentMacroRegisterHotkeys->setChecked(
			macro->PauseHotkeysEnabled());
		_currentMacroRegisterDock->setChecked(macro->DockEnabled());
	} else {
		hotkeyOptions->hide();
		dockOptions->hide();
	}
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
	macro->EnableDock(dialog._currentMacroRegisterDock->isChecked());
	return true;
}
