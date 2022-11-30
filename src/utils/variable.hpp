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
	~Variable();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string Value() const { return _value; }
	bool DoubleValue(double &) const;
	void SetValue(const std::string &val);
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

// Helper class which automatically resovles variables contained in strings
// when reading its value as a std::string
class VariableResolvingString {
public:
	VariableResolvingString() : _value(""){};
	VariableResolvingString(std::string str) : _value(std::move(str)){};
	VariableResolvingString(const char *str) : _value(str){};
	operator std::string();
	void operator=(std::string);
	void operator=(const char *value);
	const char *c_str();
	const char *c_str() const;

	const std::string &UnresolvedValue() const { return _value; }

	void Load(obs_data_t *obj, const char *name);
	void Save(obs_data_t *obj, const char *name) const;

private:
	void Resolve();

	std::string _value = "";
	std::string _resolvedValue = "";
	std::chrono::high_resolution_clock::time_point _lastResolve{};
};

Variable *GetVariableByName(const std::string &name);
Variable *GetVariableByQString(const QString &name);
std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name);
std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name);
QStringList GetVariablesNameList();
std::string SubstitueVariables(std::string str);

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
