#include "macro-action-variable-basic.hpp"

namespace advss {

using Action = MacroActionVariable::Action;

static bool registered = RegisterVariableActionGroup(
	{0,
	 {
		 {Action::SET_VALUE, "AdvSceneSwitcher.action.variable.type.set",
		  "AdvSceneSwitcher.action.variable.layout.strValue", true},
		 {Action::COPY_VAR, "AdvSceneSwitcher.action.variable.type.copy",
		  "AdvSceneSwitcher.action.variable.layout.copy"},
		 {Action::APPEND, "AdvSceneSwitcher.action.variable.type.append",
		  "AdvSceneSwitcher.action.variable.layout.strValue", true},
		 {Action::APPEND_VAR,
		  "AdvSceneSwitcher.action.variable.type.appendVar",
		  "AdvSceneSwitcher.action.variable.layout.variable2"},
		 {Action::SWAP_VALUES,
		  "AdvSceneSwitcher.action.variable.type.swapValues",
		  "AdvSceneSwitcher.action.variable.layout.variable2"},
	 },
	 []() { return std::make_unique<VariableActionBasic>(); },
	 VariableActionBasicEdit::Create});

static void apppend(Variable &var, const std::string &value)
{
	auto currentValue = var.Value();
	var.SetValue(currentValue + value);
}

std::unique_ptr<VariableActionGroup> VariableActionBasic::Copy() const
{
	return std::make_unique<VariableActionBasic>(*this);
}

bool VariableActionBasic::Perform(MacroActionVariable &action,
				  MacroActionVariable::Action type,
				  Variable &var)
{
	switch (type) {
	case Action::SET_VALUE:
		var.SetValue(action._strValue);
		break;
	case Action::APPEND:
		apppend(var, action._strValue);
		break;
	case Action::COPY_VAR: {
		auto var2 = action._variable2.lock();
		if (!var2) {
			return true;
		}
		var.SetValue(var2->Value());
		break;
	}
	case Action::APPEND_VAR: {
		auto var2 = action._variable2.lock();
		if (!var2) {
			return true;
		}
		apppend(var, var2->Value());
		break;
	}
	case Action::SWAP_VALUES: {
		auto var2 = action._variable2.lock();
		if (!var2) {
			return true;
		}
		auto tempValue = var.Value();
		var.SetValue(var2->Value());
		var2->SetValue(tempValue);
		break;
	}
	default:
		break;
	}
	return true;
}

VariableActionBasicEdit::VariableActionBasicEdit(
	QWidget *parent, std::shared_ptr<MacroActionVariable> entryData)
	: VariableActionGroupEdit(parent, entryData)
{
	_loading = false;
}

VariableActionGroupEdit *
VariableActionBasicEdit::Create(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData)
{
	return new VariableActionBasicEdit(parent, entryData);
}

void VariableActionBasicEdit::SetWidgetVisibility(MacroActionVariable::Action)
{
	hide();
}

} // namespace advss
