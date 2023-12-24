#pragma once
#include "item-selection-helpers.hpp"
#include "resizing-text-edit.hpp"

#include <string>
#include <optional>
#include <QStringList>
#include <obs.hpp>

namespace advss {

class VariableSelection;
class VariableSettingsDialog;

class Variable : public Item {
public:
	Variable();
	~Variable();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	std::string Value() const { return _value; }
	std::optional<double> DoubleValue() const;
	std::optional<int> IntValue() const;
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

class VariableSettingsDialog : public ItemSettingsDialog {
	Q_OBJECT

public:
	VariableSettingsDialog(QWidget *parent, const Variable &);
	static bool AskForSettings(QWidget *parent, Variable &settings);

private slots:
	void SaveActionChanged(int);

private:
	ResizingPlainTextEdit *_value;
	ResizingPlainTextEdit *_defaultValue;
	QComboBox *_save;
};

class VariableSelection : public ItemSelection {
	Q_OBJECT

public:
	VariableSelection(QWidget *parent = 0);
	void SetVariable(const std::string &);
	void SetVariable(const std::weak_ptr<Variable> &);
};

void SaveVariables(obs_data_t *obj);
void LoadVariables(obs_data_t *obj);

std::deque<std::shared_ptr<Item>> &GetVariables();

Variable *GetVariableByName(const std::string &name);
Variable *GetVariableByQString(const QString &name);
std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name);
std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name);
std::string GetWeakVariableName(std::weak_ptr<Variable>);
QStringList GetVariablesNameList();

std::chrono::high_resolution_clock::time_point GetLastVariableChangeTime();

} // namespace advss
