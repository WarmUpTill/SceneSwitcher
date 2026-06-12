#pragma once
#include "macro-action-edit.hpp"
#include "duration-control.hpp"
#include "regex-config.hpp"
#include "variable-spinbox.hpp"
#include "variable-text-edit.hpp"
#include "variable.hpp"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace advss {

class MacroActionWait : public MacroAction {
public:
	MacroActionWait(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m);
	std::shared_ptr<MacroAction> Copy() const;
	void ResolveVariablesToFixedValues();
	void SetupTempVars();

	enum class Type {
		FIXED,
		RANDOM,
		VARIABLE_WAIT,
	};
	Type _waitType = Type::FIXED;

	// FIXED / RANDOM
	Duration _duration;
	Duration _duration2;

	// VARIABLE_WAIT
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
	Duration _conditionTimeout;

private:
	bool PerformDurationWait();
	bool PerformVariableWait();
	bool ConditionIsMet() const;

	static bool _registered;
	static const std::string id;
};

class MacroActionWaitEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionWaitEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionWait> entryData = nullptr);
	void UpdateEntryData();
	void SetupFixedDurationEdit();
	void SetupRandomDurationEdit();
	void SetupVariableWaitEdit();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionWaitEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionWait>(action));
	}

private slots:
	void DurationChanged(const Duration &value);
	void Duration2Changed(const Duration &value);
	void TypeChanged(int value);
	void VariableChanged(const QString &);
	void ConditionChanged(int);
	void StrValueChanged();
	void NumValueChanged(const NumberVariable<double> &);
	void RegexChanged(const RegexConfig &);
	void UseTimeoutChanged(int);
	void ConditionTimeoutChanged(const Duration &);

signals:
	void HeaderInfoChanged(const QString &);

private:
	void SetVariableWaitWidgetVisibility();

	// Duration widgets (FIXED / RANDOM)
	DurationSelection *_duration;
	DurationSelection *_duration2;

	// Variable condition widgets (VARIABLE_WAIT)
	VariableSelection *_variable;
	QComboBox *_variableCondition;
	VariableTextEdit *_strValue;
	VariableDoubleSpinBox *_numValue;
	RegexConfigWidget *_regex;
	QCheckBox *_useTimeout;
	DurationSelection *_conditionTimeout;

	QComboBox *_waitType;
	QHBoxLayout *_mainLayout;
	QHBoxLayout *_timeoutLayout;

	std::shared_ptr<MacroActionWait> _entryData;
	bool _loading = true;
};

} // namespace advss
