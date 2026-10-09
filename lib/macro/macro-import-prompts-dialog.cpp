#include "macro-import-prompts-dialog.hpp"
#include "help-icon.hpp"
#include "obs-module-helper.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace advss {

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
		QStringList options;
		for (const auto &option : prompt.options) {
			options << QString::fromStdString(option);
		}
		auto input = new ValueInputWidget(
			prompt.type, prompt.defaultValue, options, this);
		form->addRow(QString::fromStdString(prompt.label), input);
		_rows.push_back({prompt, input});
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
		auto value = row.input->Value();
		for (const auto &placeholder : row.prompt.placeholders) {
			result[placeholder] = value;
		}
	}
	return result;
}

} // namespace advss
