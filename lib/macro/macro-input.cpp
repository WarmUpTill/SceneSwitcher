#include "macro-input.hpp"
#include "layout-helpers.hpp"
#include "log-helper.hpp"
#include "obs-module-helper.hpp"
#include "variable-line-edit.hpp"

#include <algorithm>
#include <obs.hpp>

namespace advss {

void MacroInputVariables::Save(obs_data_t *obj) const
{
	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (auto &var : _inputVariables) {
		OBSDataAutoRelease arrayObj = obs_data_create();
		auto variable = var.lock();
		if (variable) {
			obs_data_set_string(arrayObj, "variableName",
					    variable->Name().c_str());
		} else {
			obs_data_set_bool(arrayObj, "invalidVariable", true);
		}
		obs_data_array_push_back(array, arrayObj);
	}
	obs_data_set_array(obj, "inputVariables", array);
}

void MacroInputVariables::Load(obs_data_t *obj)
{
	OBSDataArrayAutoRelease array =
		obs_data_get_array(obj, "inputVariables");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease arrayObj = obs_data_array_item(array, i);
		const bool isInvalid =
			obs_data_get_bool(arrayObj, "invalidVariable");
		if (isInvalid) {
			_inputVariables.emplace_back();
		} else {
			const auto name =
				obs_data_get_string(arrayObj, "variableName");
			_inputVariables.emplace_back(
				GetWeakVariableByName(name));
		}
	}
}

void MacroInputVariables::SetValues(const StringList &values)
{
	if (_inputVariables.size() != (size_t)values.size()) {
		blog(LOG_WARNING,
		     "%s - sizes do not match (variables: %d, values %d)",
		     __func__, (int)_inputVariables.size(), (int)values.size());
	}

	for (size_t index = 0;
	     index < _inputVariables.size() && index < (size_t)values.size();
	     index++) {
		auto variable = _inputVariables.at(index).lock();
		if (!variable) {
			continue;
		}

		variable->SetValue(values.at(index));
	}
}

MacroInputSelection::MacroInputSelection(QWidget *parent) : ListEditor(parent)
{
	_list->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

void MacroInputSelection::SetInputs(const MacroInputVariables &inputs)
{
	_currentSelection = inputs;
	for (const auto &var : inputs._inputVariables) {
		auto variable = var.lock();
		const auto name =
			variable
				? QString::fromStdString(variable->Name())
				: obs_module_text(
					  "AdvSceneSwitcher.macroTab.inputSettings.invalid");

		QVariant v = QVariant::fromValue(name);
		auto item = new QListWidgetItem(name, _list);
		item->setData(Qt::UserRole, name);
	}
	UpdateListSize();
}

void MacroInputSelection::Add()
{
	std::string varName;
	bool accepted = VariableSelectionDialog::AskForVariable(this, varName);

	if (!accepted || varName.empty()) {
		return;
	}

	_currentSelection._inputVariables.emplace_back(
		GetWeakVariableByName(varName));

	QVariant v = QVariant::fromValue(QString::fromStdString(varName));
	auto item = new QListWidgetItem(QString::fromStdString(varName), _list);
	item->setData(Qt::UserRole, QString::fromStdString(varName));
	UpdateListSize();
}

void MacroInputSelection::Remove()
{
	auto item = _list->currentItem();
	int idx = _list->currentRow();
	if (!item || idx == -1) {
		return;
	}

	_currentSelection._inputVariables.erase(
		std::next(_currentSelection._inputVariables.begin(), idx));

	delete item;
	UpdateListSize();
}

void MacroInputSelection::Up()
{
	int idx = _list->currentRow();
	if (idx < 1) {
		return;
	}

	_list->insertItem(idx - 1, _list->takeItem(idx));
	_list->setCurrentRow(idx - 1);

	std::iter_swap(
		std::next(_currentSelection._inputVariables.begin(), idx),
		std::next(_currentSelection._inputVariables.begin(), idx - 1));
}

void MacroInputSelection::Down()
{
	int idx = _list->currentRow();
	if (idx < 0 || idx >= _list->count()) {
		return;
	}

	_list->insertItem(idx + 1, _list->takeItem(idx));
	_list->setCurrentRow(idx + 1);

	std::iter_swap(
		std::next(_currentSelection._inputVariables.begin(), idx + 1),
		std::next(_currentSelection._inputVariables.begin(), idx));
}

void MacroInputSelection::Clicked(QListWidgetItem *item)
{
	std::string varName;
	bool accepted = VariableSelectionDialog::AskForVariable(this, varName);

	if (!accepted || varName.empty()) {
		return;
	}

	item->setText(QString::fromStdString(varName));
	int idx = _list->currentRow();

	_currentSelection._inputVariables.at(idx) =
		GetWeakVariableByName(varName);
}

MacroInputEdit::MacroInputEdit(QWidget *parent)
	: QWidget(parent),
	  _layout(new QGridLayout())
{
	setLayout(_layout);
}

void MacroInputEdit::SetInputVariablesAndValues(
	const MacroInputVariables &inputs, const StringList &values)
{
	_variables = inputs;
	_values = values;
	if ((size_t)_values.size() < _variables._inputVariables.size()) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		for (int i = 0;
		     _variables._inputVariables.size() - (size_t)_values.size();
		     i++) {
			_values.push_back({});
		}
#else
		_values.resize(_variables._inputVariables.size());
#endif
	}
	SetupWidgets();
}

bool MacroInputEdit::HasInputsToSet() const
{
	return !_variables._inputVariables.empty();
}

void HighligthMacroSettingsButton(bool enable);

void MacroInputEdit::SetupWidgets()
{
	ClearLayout(_layout);

	auto &vars = _variables._inputVariables;
	if (vars.empty()) {
		_layout->addWidget(new QLabel(obs_module_text(
			"AdvSceneSwitcher.macroTab.inputSettings.noInputs")));
		adjustSize();
		updateGeometry();
		return;
	}

	for (size_t index = 0; index < vars.size(); index++) {
		auto variable = vars.at(index).lock();
		QLabel *label = new QLabel(
			variable
				? QString::fromStdString(variable->Name())
				: obs_module_text(
					  "AdvSceneSwitcher.macroTab.inputSettings.invalid"));
		_layout->addWidget(label, index, 0);
		auto valueEdit = new VariableLineEdit(this);
		valueEdit->setEnabled(!!variable);
		valueEdit->setText(_values.at(index));
		connect(valueEdit, &VariableLineEdit::editingFinished,
			[this, valueEdit, index]() {
				_values[index] =
					valueEdit->text().toStdString();
				emit MacroInputValuesChanged(_values);
			});
		_layout->addWidget(valueEdit, index, 1);
	}

	adjustSize();
	updateGeometry();
}

} // namespace advss
