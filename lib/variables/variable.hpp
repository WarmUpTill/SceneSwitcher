#pragma once
#include "export-symbol-helper.hpp"
#include "item-selection-helpers.hpp"
#include "resizing-text-edit.hpp"

#include <string>
#include <optional>
#include <QStringList>
#include <obs-data.h>

namespace advss {

class VariableSelection;
class VariableSettingsDialog;

class Variable : public Item {
public:
	Variable();
	~Variable();
	void Load(obs_data_t *obj);
	void Save(obs_data_t *obj) const;
	EXPORT std::string Value(bool updateLastUsed = true) const;
	EXPORT std::optional<double> DoubleValue() const;
	EXPORT std::optional<int> IntValue() const;
	void SetValue(const std::string &value);
	void SetValue(double value);
	std::string GetDefaultValue() const { return _defaultValue; }
	static std::shared_ptr<Item> Create()
	{
		return std::make_shared<Variable>();
	}

	enum class SaveAction {
		DONT_SAVE,
		SAVE,
		SET_DEFAULT,
	};
	SaveAction GetSaveAction() const { return _saveAction; }
	std::optional<uint64_t> SecondsSinceLastUse() const;
	void UpdateLastUsed() const;

private:
	SaveAction _saveAction = SaveAction::DONT_SAVE;
	std::string _value = "";
	std::string _defaultValue = "";
	mutable std::chrono::high_resolution_clock::time_point _lastUsed;

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

void SaveVariables(obs_data_t *obj);
void LoadVariables(obs_data_t *obj);
void ImportVariables(obs_data_t *obj);

std::deque<std::shared_ptr<Item>> &GetVariables();

EXPORT Variable *GetVariableByName(const std::string &name);
EXPORT Variable *GetVariableByQString(const QString &name);
EXPORT std::weak_ptr<Variable> GetWeakVariableByName(const std::string &name);
EXPORT std::weak_ptr<Variable> GetWeakVariableByQString(const QString &name);
std::string GetWeakVariableName(std::weak_ptr<Variable>);
EXPORT QStringList GetVariablesNameList();

std::chrono::high_resolution_clock::time_point GetLastVariableChangeTime();

} // namespace advss
