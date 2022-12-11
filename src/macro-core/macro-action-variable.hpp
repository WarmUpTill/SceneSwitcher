#pragma once
#include "macro-action-edit.hpp"
#include "variable.hpp"

class MacroActionVariable : public MacroAction {
public:
	MacroActionVariable(Macro *m) : MacroAction(m) {}
	bool PerformAction();
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string GetShortDesc() const;
	std::string GetId() const { return id; };
	static std::shared_ptr<MacroAction> Create(Macro *m)
	{
		return std::make_shared<MacroActionVariable>(m);
	}

	enum class Type {
		SET,
		APPEND,
		APPEND_VAR,
		INCREMENT,
		DECREMENT,
		//...
	};

	Type _type = Type::SET;
	std::string _variableName = "";
	std::string _variable2Name = "";
	std::string _strValue = "";
	double _numValue = 0;

private:
	static bool _registered;
	static const std::string id;
};

class MacroActionVariableEdit : public QWidget {
	Q_OBJECT

public:
	MacroActionVariableEdit(
		QWidget *parent,
		std::shared_ptr<MacroActionVariable> entryData = nullptr);
	void UpdateEntryData();
	static QWidget *Create(QWidget *parent,
			       std::shared_ptr<MacroAction> action)
	{
		return new MacroActionVariableEdit(
			parent,
			std::dynamic_pointer_cast<MacroActionVariable>(action));
	}

private slots:
	void VariableChanged(const QString &);
	void Variable2Changed(const QString &);
	void ActionChanged(int);
	void StrValueChanged();
	void NumValueChanged(double);

signals:
	void HeaderInfoChanged(const QString &);

protected:
	VariableSelection *_variables;
	VariableSelection *_variables2;
	QComboBox *_actions;
	QLineEdit *_strValue;
	QDoubleSpinBox *_numValue;
	std::shared_ptr<MacroActionVariable> _entryData;

private:
	void SetWidgetVisibility();

	bool _loading = true;
};
