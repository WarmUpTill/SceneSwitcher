#include "macro-action-macro.hpp"

namespace advss {

bool MacroActionMacro::_registered = false;
const std::string MacroActionMacro::id = "macro";

bool MacroActionMacro::PerformAction()
{
	return false;
}

void MacroActionMacro::LogAction() const {}

bool MacroActionMacro::Save(obs_data_t *obj) const
{
	return false;
}

bool MacroActionMacro::Load(obs_data_t *obj)
{
	return false;
}

bool MacroActionMacro::PostLoad()
{
	return false;
}

std::string MacroActionMacro::GetShortDesc() const
{
	return "";
}

std::shared_ptr<MacroAction> MacroActionMacro::Create(Macro *m)
{
	return std::make_shared<MacroActionMacro>(m);
}

std::shared_ptr<MacroAction> MacroActionMacro::Copy() const
{
	return std::make_shared<MacroActionMacro>(*this);
}

void MacroActionMacro::ResolveVariablesToFixedValues() {}

void MacroActionMacro::RunOptions::Save(obs_data_t *obj) const {}

void MacroActionMacro::RunOptions::Load(obs_data_t *obj) {}

void MacroActionMacro::RunActions(Macro *actionMacro) const {}

void MacroActionMacro::AdjustActionState(Macro *macro) const {}

MacroActionMacroEdit::MacroActionMacroEdit(
	QWidget *parent, std::shared_ptr<MacroActionMacro> entryData)
	: ResizableWidget(parent),
	  _actions(new QComboBox(this)),
	  _macros(new MacroSelection(this)),
	  _actionSelectionType(new QComboBox(this)),
	  _label(new VariableLineEdit(this)),
	  _actionTypes(new FilterComboBox(this)),
	  _regex(new RegexConfigWidget(this)),
	  _conditionMacros(new MacroSelection(this)),
	  _conditionBehaviors(new QComboBox(this)),
	  _reevaluateConditionState(new QCheckBox(this)),
	  _actionSections(new QComboBox(this)),
	  _skipWhenPaused(new QCheckBox(this)),
	  _setInputs(new QCheckBox(this)),
	  _inputs(new MacroInputEdit(this)),
	  _entryLayout(new QHBoxLayout()),
	  _conditionLayout(new QHBoxLayout()),
	  _reevaluateConditionStateLayout(new QHBoxLayout()),
	  _setInputsLayout(new QHBoxLayout()),
	  _nestedMacro(new MacroEdit(this)),
	  _entryData(entryData)
{
}

MacroActionMacroEdit::~MacroActionMacroEdit() {}

void MacroActionMacroEdit::UpdateEntryData() {}

QWidget *MacroActionMacroEdit::Create(QWidget *parent,
				      std::shared_ptr<MacroAction> action)
{
	return new MacroActionMacroEdit(
		parent, std::dynamic_pointer_cast<MacroActionMacro>(action));
}

void MacroActionMacroEdit::MacroChanged(const QString &text) {}

void MacroActionMacroEdit::ActionChanged(int value) {}

void MacroActionMacroEdit::ActionSelectionTypeChanged(int value) {}

void MacroActionMacroEdit::ActionIndexChanged(const IntVariable &value) {}

void MacroActionMacroEdit::LabelChanged() {}

void MacroActionMacroEdit::ActionTypeChanged(int value) {}

void MacroActionMacroEdit::RegexChanged(const RegexConfig &config) {}

void MacroActionMacroEdit::ConditionMacroChanged(const QString &text) {}

void MacroActionMacroEdit::ConditionBehaviorChanged(int value) {}

void MacroActionMacroEdit::ReevaluateConditionStateChanged(int value) {}

void MacroActionMacroEdit::ActionSectionChanged(int value) {}

void MacroActionMacroEdit::SkipWhenPausedChanged(int value) {}

void MacroActionMacroEdit::SetInputsChanged(int value) {}

void MacroActionMacroEdit::InputsChanged(const StringList &inputs) {}

void MacroActionMacroEdit::SetWidgetVisibility() {}

void MacroActionMacroEdit::SetupMacroInput(Macro *macro) const {}

} // namespace advss
