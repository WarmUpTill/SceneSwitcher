#pragma once
#include "macro.hpp"
#include "resizing-text-edit.hpp"
#include "variable.hpp"

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>

class MacroConditionVariable : public MacroCondition {
public:
	MacroConditionVariable(Macro *m) : MacroCondition(m) {}
	bool CheckCondition();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroCondition> Create(Macro *m)
	{
		return std::make_shared<MacroConditionVariable>(m);
	}

	enum class Type {
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

	Type _type = Type::EQUALS;
	std::string _variableName = "";
	std::string _variable2Name = "";
	std::string _strValue = "";
	double _numValue = 0;
	bool _regex = false;

private:
	bool Compare(const Variable &) const;
	bool ValueChanged(const Variable &);
	bool CompareVariables();

	std::string _lastValue = "";

	static bool _registered;
	static const std::string id;
};

class MacroConditionVariableEdit : public QWidget {
	Q_OBJECT

public:
	MacroConditionVariableEdit(
		QWidget *parent,
		std::shared_ptr<MacroConditionVariable> cond = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroCondition> cond)
	{
		return new MacroConditionVariableEdit(
			parent,
			std::dynamic_pointer_cast<MacroConditionVariable>(
				cond));
	}

private slots:
	void VariableChanged(const QString &);
	void Variable2Changed(const QString &);
	void ConditionChanged(int);
	void StrValueChanged();
	void NumValueChanged(double);
	void RegexChanged(int state);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	VariableSelection *_variables;
	VariableSelection *_variables2;
	QComboBox *_conditions;
	ResizingPlainTextEdit *_strValue;
	QDoubleSpinBox *_numValue;
	QCheckBox *_regex;
	std::shared_ptr<MacroConditionVariable> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
