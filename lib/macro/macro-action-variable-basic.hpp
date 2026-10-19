#pragma once
#include "macro-action-variable-group.hpp"

namespace advss {

class VariableActionBasic : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;
};

class VariableActionBasicEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionBasicEdit(QWidget *parent,
				std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	void UpdateEntryData() override {}
	void SetWidgetVisibility(MacroActionVariable::Action) override;
};

} // namespace advss
