#include "macro-properties.hpp"

#include <QVBoxLayout>
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
		  "AdvSceneSwitcher.macroTab.currentDisableHotkeys")))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);

	_executed->setChecked(prop._highlightExecuted);
	_conditions->setChecked(prop._highlightConditions);
	_actions->setChecked(prop._highlightActions);
	_newMacroRegisterHotkeys->setChecked(prop._newMacroRegisterHotkeys);
	if (macro) {
		_currentMacroRegisterHotkeys->setChecked(
			macro->PauseHotkeysEnabled());
	} else {
		_currentMacroRegisterHotkeys->hide();
	}

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_executed);
	layout->addWidget(_conditions);
	layout->addWidget(_actions);
	layout->addWidget(_newMacroRegisterHotkeys);
	if (macro) {
		QFrame *line = new QFrame();
		line->setFrameShape(QFrame::HLine);
		line->setFrameShadow(QFrame::Sunken);
		layout->addWidget(line);
	}
	layout->addWidget(_currentMacroRegisterHotkeys);
	setLayout(layout);

	QDialogButtonBox *buttonbox = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	layout->addWidget(buttonbox);
	buttonbox->setCenterButtons(true);
	connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
	if (macro) {
		macro->EnablePauseHotkeys(
			dialog._currentMacroRegisterHotkeys->isChecked());
	}
	return true;
}
