#pragma once
#include "macro-action-variable-group.hpp"
#include "string-list.hpp"
#include "variable-line-edit.hpp"
#include "variable-spinbox.hpp"

#include <optional>

namespace advss {

class VariableActionMath : public VariableActionGroup {
public:
	std::unique_ptr<VariableActionGroup> Copy() const override;
	void Save(obs_data_t *, const MacroActionVariable &) const override;
	void Load(obs_data_t *, MacroActionVariable &) override;
	void ResolveVariables() override;
	bool Perform(MacroActionVariable &, MacroActionVariable::Action,
		     Variable &) override;

	DoubleVariable _numValue = 0;
	StringVariable _mathExpression = obs_module_text(
		"AdvSceneSwitcher.action.variable.mathExpression.example");
	DoubleVariable _randomNumberStart = 0;
	DoubleVariable _randomNumberEnd = 100;
	bool _generateInteger = true;
	StringList _randomValues = {"value1", "value2", "value3"};
	bool _allowRepeatValues = true;

private:
	void HandleMathExpression(Variable &);
	void GenerateRandomNumber(Variable &);
	void PickRandomValue(Variable &);

	std::optional<std::string> _lastRandomValue;
};

class VariableActionMathEdit : public VariableActionGroupEdit {
	Q_OBJECT

public:
	VariableActionMathEdit(QWidget *parent,
			       std::shared_ptr<MacroActionVariable> entryData);
	static VariableActionGroupEdit *
	Create(QWidget *parent, std::shared_ptr<MacroActionVariable> entryData);
	std::unordered_map<std::string, QWidget *> GetInlineWidgets() override;
	void UpdateEntryData() override;
	void SetWidgetVisibility(MacroActionVariable::Action) override;

private slots:
	void NumValueChanged(const NumberVariable<double> &value);
	void MathExpressionChanged();
	void RandomNumberStartChanged(const NumberVariable<double> &);
	void RandomNumberEndChanged(const NumberVariable<double> &);
	void GenerateIntegerChanged(int);
	void RandomValueListChanged(const StringList &);
	void AllowRepeatValuesChanged(int);

private:
	VariableActionMath *_data;

	VariableDoubleSpinBox *_numValue;
	VariableLineEdit *_mathExpression;
	QLabel *_mathExpressionResult;
	VariableDoubleSpinBox *_randomNumberStart;
	VariableDoubleSpinBox *_randomNumberEnd;
	QCheckBox *_generateInteger;
	QVBoxLayout *_randomNumberLayout;
	StringListEdit *_randomValues;
	QCheckBox *_allowRepeatValues;
	QVBoxLayout *_randomValueLayout;
};

} // namespace advss
