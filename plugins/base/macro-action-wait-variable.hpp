#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "regex-config.hpp"
#include "variable-spinbox.hpp"
#include "variable-text-edit.hpp"
#include "variable.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>

namespace advss {

class MacroActionWaitVariable : public MacroAction {
public:
	MacroActionWaitVariable(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; }
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();

	enum class Condition {
		EQUALS,
		DOES_NOT_EQUAL,
		IS_EMPTY,
		LESS_THAN,
		GREATER_THAN,
	};

	std::weak_ptr<Variable> _variable;
	Condition _condition = Condition::EQUALS;
	StringVariable _strValue = "";
	DoubleVariable _numValue = 0.0;
	RegexConfig _regex;
	bool _useTimeout = false;
	Duration _timeout;

private:
	bool ConditionIsMet() const;

	static bool _registered;
	static const std::string id;
};

class MacroActionWaitVariableEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWaitVariableEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWaitVariable> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitVariableEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWaitVariable>(
				action));
	}

private slots:
	void VariableChanged(const QString &);
	void ConditionChanged(int);
	void StrValueChanged();
	void NumValueChanged(const NumberVariable<double> &);
	void RegexChanged(const RegexConfig &);
	void UseTimeoutChanged(int);
	void TimeoutChanged(const Duration &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetWidgetVisibility();

	VariableSelection *_variable;
	QComboBox *_condition;
	VariableTextEdit *_strValue;
	VariableDoubleSpinBox *_numValue;
	RegexConfigWidget *_regex;
	QCheckBox *_useTimeout;
	DurationSelection *_timeout;
	QHBoxLayout *_mainLayout;
	QHBoxLayout *_timeoutLayout;

	std::shared_ptr<MacroActionWaitVariable> _entryData;
	bool _loading = true;
};

} // namespace advss
