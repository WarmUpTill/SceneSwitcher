#include "macro-export-prompts-dialog.hpp"
#include "json-helpers.hpp"
#include "obs-module-helper.hpp"

#include <nlohmann/json.hpp>
#include <obs.hpp>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace advss {

static std::string getArrayElementJson(const std::string &arrayJson, int index)
{
	try {
		auto j = nlohmann::json::parse(arrayJson);
		if (!j.is_array() || index < 0 || index >= (int)j.size()) {
			return "{}";
		}
		return j.at(index).dump();
	} catch (const nlohmann::json::exception &) {
		return "{}";
	}
}

static std::string setArrayElementJson(const std::string &arrayJson, int index,
				       const std::string &elementJson)
{
	try {
		auto j = nlohmann::json::parse(arrayJson);
		if (j.is_array() && index >= 0 && index < (int)j.size()) {
			j[index] = nlohmann::json::parse(elementJson);
		}
		return j.dump();
	} catch (const nlohmann::json::exception &) {
		return arrayJson;
	}
}

static int arraySize(const std::string &arrayJson)
{
	try {
		auto j = nlohmann::json::parse(arrayJson);
		return j.is_array() ? (int)j.size() : 0;
	} catch (const nlohmann::json::exception &) {
		return 0;
	}
}

void ExportPromptsState::Load(obs_data_t *data)
{
	OBSDataArrayAutoRelease macros = obs_data_get_array(data, "macros");
	OBSDataAutoRelease wrapper = obs_data_create();
	obs_data_set_array(wrapper, "macros", macros);
	macrosArrayJson = GetJsonField(obs_data_get_json(wrapper), "macros")
				  .value_or("[]");
	entries.clear();
}

void ExportPromptsState::Apply(obs_data_t *data) const
{
	if (entries.empty()) {
		return;
	}

	std::string wrapped = "{\"macros\":" + macrosArrayJson + "}";
	OBSDataAutoRelease wrapper = obs_data_create_from_json(wrapped.c_str());
	if (!wrapper) {
		return;
	}
	OBSDataArrayAutoRelease macros = obs_data_get_array(wrapper, "macros");
	obs_data_set_array(data, "macros", macros);

	std::vector<ImportPrompt> prompts;
	for (auto &entry : entries) {
		prompts.push_back(entry.prompt);
	}
	SaveImportPrompts(data, prompts);
}

MacroExportPromptsDialog::MacroExportPromptsDialog(
	QWidget *parent, const ExportPromptsState &state)
	: QDialog(parent),
	  _state(state)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));

	auto layout = new QVBoxLayout(this);

	auto info = new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.info"));
	info->setWordWrap(true);
	layout->addWidget(info);

	auto scopeLayout = new QHBoxLayout();
	scopeLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.scope")));
	_scope = new QComboBox(this);
	_scope->addItem(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.scope.all"));
	int count = arraySize(_state.macrosArrayJson);
	for (int i = 0; i < count; i++) {
		auto name = GetJsonField(getArrayElementJson(
						 _state.macrosArrayJson, i),
					 "name")
				    .value_or("");
		_scope->addItem(QString::fromStdString(name));
	}
	connect(_scope, qOverload<int>(&QComboBox::currentIndexChanged), this,
		&MacroExportPromptsDialog::ScopeChanged);
	scopeLayout->addWidget(_scope, 1);
	layout->addLayout(scopeLayout);

	auto contentLayout = new QHBoxLayout();

	auto valuesLayout = new QVBoxLayout();
	valuesLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.values")));
	_values = new QTableWidget(0, 2, this);
	_values->setHorizontalHeaderLabels(
		{obs_module_text(
			 "AdvSceneSwitcher.macroTab.export.importPrompts.value"),
		 obs_module_text(
			 "AdvSceneSwitcher.macroTab.export.importPrompts.occurrences")});
	_values->horizontalHeader()->setStretchLastSection(false);
	_values->horizontalHeader()->setSectionResizeMode(0,
							  QHeaderView::Stretch);
	_values->setEditTriggers(QAbstractItemView::NoEditTriggers);
	_values->setSelectionBehavior(QAbstractItemView::SelectRows);
	_values->setSelectionMode(QAbstractItemView::SingleSelection);
	valuesLayout->addWidget(_values);
	auto addButton = new QPushButton(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.add"),
		this);
	connect(addButton, &QPushButton::clicked, this,
		&MacroExportPromptsDialog::AddPrompt);
	valuesLayout->addWidget(addButton);
	contentLayout->addLayout(valuesLayout, 1);

	auto promptsLayout = new QVBoxLayout();
	promptsLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.defined")));
	_promptsList = new QListWidget(this);
	promptsLayout->addWidget(_promptsList);
	auto removeButton = new QPushButton(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.remove"),
		this);
	connect(removeButton, &QPushButton::clicked, this,
		&MacroExportPromptsDialog::RemovePrompt);
	promptsLayout->addWidget(removeButton);
	contentLayout->addLayout(promptsLayout, 1);

	layout->addLayout(contentLayout);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
					    QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	buttons->setCenterButtons(true);
	layout->addWidget(buttons);

	resize(600, 400);

	RefreshValuesTable();
	RefreshPromptsList();
}

std::string MacroExportPromptsDialog::CurrentScopeJson() const
{
	int idx = _scope->currentIndex();
	if (idx <= 0) {
		return _state.macrosArrayJson;
	}
	return getArrayElementJson(_state.macrosArrayJson, idx - 1);
}

void MacroExportPromptsDialog::ApplyCurrentScopeJson(const std::string &json)
{
	int idx = _scope->currentIndex();
	if (idx <= 0) {
		_state.macrosArrayJson = json;
		return;
	}
	_state.macrosArrayJson =
		setArrayElementJson(_state.macrosArrayJson, idx - 1, json);
}

void MacroExportPromptsDialog::RefreshValuesTable()
{
	auto values = CollectDistinctJsonStringValues(CurrentScopeJson());
	_values->setRowCount(0);
	for (auto &[value, occurrences] : values) {
		int row = _values->rowCount();
		_values->insertRow(row);
		_values->setItem(
			row, 0,
			new QTableWidgetItem(QString::fromStdString(value)));
		_values->setItem(
			row, 1,
			new QTableWidgetItem(QString::number(occurrences)));
	}
}

void MacroExportPromptsDialog::RefreshPromptsList()
{
	_promptsList->clear();
	for (auto &entry : _state.entries) {
		_promptsList->addItem(QString("%1 (%2)").arg(
			QString::fromStdString(entry.prompt.label),
			QString::fromStdString(entry.scope)));
	}
}

void MacroExportPromptsDialog::ScopeChanged(int)
{
	RefreshValuesTable();
}

void MacroExportPromptsDialog::AddPrompt()
{
	int row = _values->currentRow();
	if (row < 0) {
		return;
	}
	auto value = _values->item(row, 0)->text();

	QDialog dialog(this);
	dialog.setWindowTitle(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.importPrompts.add"));
	auto layout = new QVBoxLayout(&dialog);
	auto form = new QFormLayout();

	auto labelEdit = new QLineEdit(&dialog);
	form->addRow(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.label"),
		labelEdit);

	auto typeCombo = new QComboBox(&dialog);
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.file"),
		static_cast<int>(ImportPromptType::File));
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.folder"),
		static_cast<int>(ImportPromptType::Folder));
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.scene"),
		static_cast<int>(ImportPromptType::Scene));
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.source"),
		static_cast<int>(ImportPromptType::Source));
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.string"),
		static_cast<int>(ImportPromptType::String));
	typeCombo->addItem(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type.variable"),
		static_cast<int>(ImportPromptType::Variable));
	typeCombo->setCurrentIndex(typeCombo->findData(
		static_cast<int>(ImportPromptType::String)));
	form->addRow(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.type"),
		typeCombo);

	auto defaultValueEdit = new QLineEdit(value, &dialog);
	form->addRow(
		obs_module_text(
			"AdvSceneSwitcher.macroTab.export.importPrompts.defaultValue"),
		defaultValueEdit);

	layout->addLayout(form);

	auto buttons = new QDialogButtonBox(
		QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	connect(buttons, &QDialogButtonBox::accepted, &dialog,
		&QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog,
		&QDialog::reject);
	buttons->setCenterButtons(true);
	layout->addWidget(buttons);

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	auto label = labelEdit->text().toStdString();
	if (label.empty()) {
		return;
	}
	auto type =
		static_cast<ImportPromptType>(typeCombo->currentData().toInt());

	ExportPromptsState::Entry entry;
	entry.prompt.label = label;
	entry.prompt.type = type;
	entry.prompt.defaultValue = defaultValueEdit->text().toStdString();
	entry.scope = _scope->currentText().toStdString();

	auto placeholder = GenerateImportPlaceholder();
	ApplyCurrentScopeJson(ReplaceJsonStringValue(
		CurrentScopeJson(), value.toStdString(), placeholder));
	entry.prompt.placeholders.push_back(placeholder);
	entry.originalValues.push_back(value.toStdString());

	_state.entries.emplace_back(std::move(entry));

	RefreshValuesTable();
	RefreshPromptsList();
}

void MacroExportPromptsDialog::RemovePrompt()
{
	int idx = _promptsList->currentRow();
	if (idx < 0 || idx >= (int)_state.entries.size()) {
		return;
	}

	auto &entry = _state.entries[idx];
	for (size_t i = 0; i < entry.prompt.placeholders.size(); i++) {
		_state.macrosArrayJson = ReplaceJsonStringValue(
			_state.macrosArrayJson, entry.prompt.placeholders[i],
			entry.originalValues[i]);
	}

	_state.entries.erase(_state.entries.begin() + idx);
	RefreshValuesTable();
	RefreshPromptsList();
}

bool MacroExportPromptsDialog::Edit(QWidget *parent, ExportPromptsState &state)
{
	MacroExportPromptsDialog dialog(parent, state);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}
	state = dialog._state;
	return true;
}

} // namespace advss
