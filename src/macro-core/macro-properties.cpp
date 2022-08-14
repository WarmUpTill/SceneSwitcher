#include "macro-properties.hpp"

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <obs-module.h>

void MacroProperties::Save(obs_data_t *obj)
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "highlightExecuted", _highlightExecuted);
	obs_data_set_bool(data, "highlightConditions", _highlightConditions);
	obs_data_set_bool(data, "highlightActions", _highlightActions);
	obs_data_set_obj(obj, "macroProperties", data);
	obs_data_release(data);
}

void MacroProperties::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "macroProperties");
	// TODO: Remove in future version
	if (obs_data_has_user_value(obj, "highlightExecutedMacros")) {
		_highlightExecuted =
			obs_data_get_bool(obj, "highlightExecutedMacros");
	} else {
		_highlightExecuted =
			obs_data_get_bool(data, "highlightExecuted");
	}
	_highlightConditions = obs_data_get_bool(data, "highlightConditions");
	_highlightActions = obs_data_get_bool(data, "highlightActions");
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
	  _hotkeys(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.macroTab.disableHotkeys")))
{
	setModal(true);
	setWindowModality(Qt::WindowModality::WindowModal);
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setFixedWidth(555);
	setMinimumHeight(100);

	_executed->setChecked(prop._highlightExecuted);
	_conditions->setChecked(prop._highlightConditions);
	_actions->setChecked(prop._highlightActions);
	if (macro) {
		_hotkeys->setChecked(macro->PauseHotkeysEnabled());
	} else {
		_hotkeys->hide();
	}

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_executed);
	layout->addWidget(_conditions);
	layout->addWidget(_actions);
	layout->addWidget(_hotkeys);
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
	if (macro) {
		macro->EnablePauseHotkeys(dialog._hotkeys->isChecked());
	}
	return true;
}
