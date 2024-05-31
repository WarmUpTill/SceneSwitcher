#pragma once
#include "list-editor.hpp"
#include "string-list.hpp"
#include "variable.hpp"

#include <QGridLayout>
#include <memory>
#include <vector>

namespace advss {

class MacroInputVariables {
public:
	void Save(obs_data_t *obj) const;
	void Load(obs_data_t *obj);
	void SetValues(const StringList &values);

private:
	std::vector<std::weak_ptr<Variable>> _inputVariables;
	friend class MacroInputSelection;
	friend class MacroInputEdit;
};

class MacroInputSelection final : public ListEditor {
	Q_OBJECT

public:
	MacroInputSelection(QWidget *parent = nullptr);
	void SetInputs(const MacroInputVariables &inputs);
	MacroInputVariables GetInputs() const { return _currentSelection; }

private slots:
	void Add();
	void Remove();
	void Up();
	void Down();
	void Clicked(QListWidgetItem *);

private:
	MacroInputVariables _currentSelection;
};

class MacroInputEdit final : public QWidget {
	Q_OBJECT

public:
	MacroInputEdit(QWidget *parent = nullptr);
	void SetInputVariablesAndValues(const MacroInputVariables &inputs,
					const StringList &values);
	bool HasInputsToSet() const;

signals:
	void MacroInputValuesChanged(const StringList &);

private:
	void SetupWidgets();

	MacroInputVariables _variables;
	StringList _values;
	QGridLayout *_layout;
};

} // namespace advss
