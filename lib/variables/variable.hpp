#pragma once
#include "export-symbol-helper.hpp"
#include "item-selection-helpers.hpp"
#include "resizing-text-edit.hpp"

#include <mutex>
#include <obs-data.h>
#include <optional>
#include <string>
#include <QStringList>

namespace advss {

class VariableSelection;
class VariableSettingsDialog;

class Variable : public Item {
public:
	Variable();
	~Variable();
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<Variable>();
	}

	enum class SaveAction {
		DONT_SAVE,
		SAVE,
		SET_DEFAULT,
	};

	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	EXPORT std::string Value(bool updateLastUsed = true) const;
	EXPORT std::optional<double> DoubleValue() const;
	EXPORT std::optional<int> IntValue() const;
	std::string GetPreviousValue() const { return _previousValue; };
	std::string GetDefaultValue() const { return _defaultValue; }
	void SetValue(const std::string &value);
	void SetValue(double value);
	SaveAction GetSaveAction() const { return _saveAction; }
	int GetValueChangeCount() const { return _valueChangeCount; }
	std::optional<uint64_t> GetSecondsSinceLastUse() const;
	std::optional<uint64_t> GetSecondsSinceLastChange() const;
	void UpdateLastUsed() const;
	void UpdateLastChanged();

private:
	SaveAction _saveAction = SaveAction::DONT_SAVE;
	std::string _value = "";
	std::string _previousValue = "";
	std::string _defaultValue = "";
	int _valueChangeCount = 0;
	mutable std::chrono::high_resolution_clock::time_point _lastUsed;
	mutable std::chrono::high_resolution_clock::time_point _lastChanged;
	mutable std::mutex _mutex;

	friend VariableSelection;
	friend VariableSettingsDialog;
};

class ADVSS_EXPORT VariableSettingsDialog : public ItemSettingsDialog {
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

class ADVSS_EXPORT VariableSelection : public ItemSelection {
	Q_OBJECT

public:
	VariableSelection(QWidget *parent = 0);
	void SetVariable(const std::string &);
	void SetVariable(const std::weak_ptr<Variable> &);
};

class VariableSelectionDialog : public QDialog {
	Q_OBJECT

public:
	VariableSelectionDialog(QWidget *parent);
	static bool AskForVariable(QWidget *parent, std::string &variableName);

private:
	VariableSelection *_variableSelection;
};

class ADVSS_EXPORT VariableSignalManager : public QObject {
	Q_OBJECT
public:
	VariableSignalManager(QObject *parent = nullptr);
	static VariableSignalManager *Instance();

signals:
	void Rename(const QString &, const QString &);
	void Add(const QString &);
	void Remove(const QString &);
};

std::deque<std::shared_ptr<Item>> &GetVariables();
EXPORT Variable *GetVariableByName(const std::string &name);
EXPORT Variable *GetVariableByQString(const QString &name);
EXPORT std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name);
EXPORT std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name);
std::string GetWeakVariableName(std::weak_ptr<Variable>);
EXPORT QStringList GetVariablesNameList();

void SaveVariables(obs_data_t *obj);
void LoadVariables(obs_data_t *obj);
void ImportVariables(obs_data_t *obj);

std::chrono::high_resolution_clock::time_point GetLastVariableChangeTime();

} // namespace advss
