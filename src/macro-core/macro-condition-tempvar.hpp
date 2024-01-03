#pragma once
#include "macro-condition-edit.hpp"
#include "resizing-text-edit.hpp"
#include "variable.hpp"
#include "regex-config.hpp"
#include "variable-text-edit.hpp"

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>

namespace advss {

class MacroConditionTempVar : public MacroCondition {
public:
	MacroConditionTempVar(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionTempVar>(m);
	}

	enum class Condition {
		EQUALS,
		IS_EMPTY,
		IS_NUMBER,
		LESS_THAN,
		GREATER_THAN,
		VALUE_CHANGED,
		EQUALS_VARIABLE,
		LESS_THAN_VARIABLE,
		GREATER_THAN_VARIABLE,
	};

	Condition _type = Condition::EQUALS;
	TempVariableRef _tempVar;
	std::weak_ptr<Variable> _variable2;
	StringVariable _strValue = "";
	double _numValue = 0;
	RegexConfig _regex;

private:
	bool Compare(const TempVariable &) const;
	bool ValueChanged(const TempVariable &);
	bool CompareVariables();

	std::string _lastValue = "";

	static bool _registered;
	static const std::string id;
};

class MacroConditionTempVarEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionTempVarEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionTempVar> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionTempVarEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionTempVar>(cond));
	}

private slots:
	void VariableChanged(const TempVariableRef &);
	void Variable2Changed(const QString &);
	void ConditionChanged(int);
	void StrValueChanged();
	void NumValueChanged(double);
	void RegexChanged(const RegexConfig &);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	TempVariableSelection *_tempVars;
	VariableSelection *_variables2;
	QComboBox *_conditions;
	VariableTextEdit *_strValue;
	QDoubleSpinBox *_numValue;
	RegexConfigWidget *_regex;
	std::shared_ptr<MacroConditionTempVar> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};

} // namespace advss
