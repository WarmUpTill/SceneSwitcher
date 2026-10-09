#include "macro-import-prompts-dialog.hpp"
#include "file-selection.hpp"
#include "help-icon.hpp"
#include "obs-module-helper.hpp"
#include "scene-selection.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"
#include "source-selection.hpp"
#include "variable.hpp"

#include <memory>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace advss {

static QStringList populateSources()
{
	auto sources = GetSourceNames();
	sources.sort();
	return sources;
}

MacroImportPromptsDialog::MacroImportPromptsDialog(
	QWidget *parent, const std::vector<ImportPrompt> &prompts)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));

	auto layout = new QVBoxLayout(this);

	auto headerLayout = new QHBoxLayout();
	headerLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.import.prompts.header")));
	headerLayout->addWidget(new HelpIcon(
		obs_module_text("AdvSceneSwitcher.macroTab.import.prompts.info"),
		this));
	headerLayout->addStretch();
	layout->addLayout(headerLayout);

	auto form = new QFormLayout();
	for (const auto &prompt : prompts) {
		Row row;
		row.prompt = prompt;
		auto label = QString::fromStdString(prompt.label);

		switch (prompt.type) {
		case ImportPromptType::File:
		case ImportPromptType::Folder: {
			auto widget = new FileSelection(
				prompt.type == ImportPromptType::Folder
					? FileSelection::Type::FOLDER
					: FileSelection::Type::READ,
				this);
			widget->SetPath(
				QString::fromStdString(prompt.defaultValue));
			form->addRow(label, widget);
			row.getValues = [widget]() {
				return std::vector<std::string>{
					widget->GetPath().toStdString()};
			};
			break;
		}
		case ImportPromptType::Scene: {
			auto widget = new SceneSelectionWidget(this);
			auto current = std::make_shared<std::string>();
			auto scene = GetWeakSourceByName(
				prompt.defaultValue.c_str());
			OBSSourceAutoRelease source =
				obs_weak_source_get_source(scene);
			if (source && obs_source_is_scene(source)) {
				SceneSelection selection;
				selection.SetScene(scene);
				widget->SetScene(selection);
				*current = prompt.defaultValue;
			}
			connect(widget, &SceneSelectionWidget::SceneChanged,
				this, [current](const SceneSelection &s) {
					*current = s.ToString();
				});
			form->addRow(label, widget);
			row.getValues = [current]() {
				return std::vector<std::string>{*current};
			};
			break;
		}
		case ImportPromptType::Source: {
			auto widget = new SourceSelectionWidget(
				this, populateSources, false);
			auto current = std::make_shared<std::string>();
			auto source = GetWeakSourceByName(
				prompt.defaultValue.c_str());
			if (source) {
				SourceSelection selection;
				selection.SetSource(source);
				widget->SetSource(selection);
				*current = prompt.defaultValue;
			}
			connect(widget, &SourceSelectionWidget::SourceChanged,
				this, [current](const SourceSelection &s) {
					*current = s.ToString();
				});
			form->addRow(label, widget);
			row.getValues = [current]() {
				return std::vector<std::string>{*current};
			};
			break;
		}
		case ImportPromptType::Variable: {
			auto widget = new VariableSelection(this);
			widget->SetVariable(prompt.defaultValue);
			form->addRow(label, widget);
			row.getValues = [widget]() {
				auto item = widget->GetCurrentItem();
				return std::vector<std::string>{
					item ? item->Name() : ""};
			};
			break;
		}
		default: // ImportPromptType::String
		{
			auto widget = new QLineEdit(
				QString::fromStdString(prompt.defaultValue),
				this);
			form->addRow(label, widget);
			row.getValues = [widget]() {
				return std::vector<std::string>{
					widget->text().toStdString()};
			};
			break;
		}
		}

		_rows.emplace_back(std::move(row));
	}
	layout->addLayout(form);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
					    QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	buttons->setCenterButtons(true);
	layout->addWidget(buttons);
}

std::unordered_map<std::string, std::string>
MacroImportPromptsDialog::GetReplacements() const
{
	std::unordered_map<std::string, std::string> result;
	for (const auto &row : _rows) {
		auto values = row.getValues();
		for (size_t i = 0;
		     i < row.prompt.placeholders.size() && i < values.size();
		     i++) {
			result[row.prompt.placeholders[i]] = values[i];
		}
	}
	return result;
}

} // namespace advss
