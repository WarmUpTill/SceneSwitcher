#pragma once
#include "item-selection-helpers.hpp"

#include <string>
#include <QStringList>
#include <obs.hpp>

class VariableSelection;
class VariableSettingsDialog;

class Variable : public Item {
public:
	Variable();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string Value() const { return _value; }
	bool DoubleValue(double &) const;
	void SetValue(const std::string &val) { _value = val; }
	void SetValue(double);
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<Variable>();
	}

	enum class SaveAction {
		DONT_SAVE,
		SAVE,
		SET_DEFAULT,
	};

private:
	SaveAction _saveAction = SaveAction::DONT_SAVE;
	std::string _value = "";
	std::string _defaultValue = "";

	friend VariableSelection;
	friend VariableSettingsDialog;
};

Variable *GetVariableByName(const std::string &name);
Variable *GetVariableByQString(const QString &name);
std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name);
std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name);
QStringList GetVariablesNameList();

class VariableSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	VariableSettingsDialog(QWidget *parent, const Variable &);
	static bool AskForSettings(QWidget *parent, Variable &settings);

private slots:
	void SaveActionChanged(int);

private:
	QLineEdit *_value;
	QLineEdit *_defaultValue;
	QComboBox *_save;
};

class VariableSelection : public ItemSelection {
	Q_OBJECT

public:
	VariableSelection(QWidget *parent = 0);
	void SetVariable(const std::string &);
};
