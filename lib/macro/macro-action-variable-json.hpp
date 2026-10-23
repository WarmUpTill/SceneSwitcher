#pragma once
#include "macro-action-variable-group.hpp"
#include "help-icon.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"

namespace advss {

class VariableActionJson : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	StringVariable _jsonQuery = "$.some.nested.value";
	IntVariable _jsonIndex = 0;
	bool _jsonQueryExtractSingle = true;
	StringVariable _jsonValue = "value";
};

class VariableActionJsonEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionJsonEdit(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void JsonQueryChanged();
	void JsonIndexChanged(const NumberVariable<int> &);
	void JsonQueryExtractSingleChanged(int);
	void JsonValueChanged();

private:
	VariableActionJson *_data;

	VariableLineEdit *_jsonQuery;
	HelpIcon *_jsonQueryHelp;
	VariableSpinBox *_jsonIndex;
	QCheckBox *_jsonExtractSingle;
	HelpIcon *_jsonExtractSingleHelp;
	VariableTextEdit *_jsonValue;
};

} // namespace advss
