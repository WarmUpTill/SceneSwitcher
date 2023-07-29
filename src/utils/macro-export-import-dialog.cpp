#include "macro-export-import-dialog.hpp"
#include "obs-module-helper.hpp"

#include <QLayout>
#include <QLabel>
#include <QDialogButtonBox>

namespace advss {

MacroExportImportDialog::MacroExportImportDialog(Type type)
	: QDialog(nullptr), _importExportString(new QPlainTextEdit(this))
{
	_importExportString->setReadOnly(type == Type::EXPORT_MACRO);
	auto label = new QLabel(obs_module_text(
		type == Type::EXPORT_MACRO
			? "AdvSceneSwitcher.macroTab.export.info"
			: "AdvSceneSwitcher.macroTab.import.info"));
	label->setWordWrap(true);

	QDialogButtonBox *buttons = nullptr;
	if (type == Type::EXPORT_MACRO) {
		buttons = new QDialogButtonBox(QDialogButtonBox::Ok);
	} else {
		buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
					       QDialogButtonBox::Cancel);
	}
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	buttons->setCenterButtons(true);

	auto layout = new QVBoxLayout;
	layout->addWidget(label);
	layout->addWidget(_importExportString);
	layout->addWidget(buttons);
	setLayout(layout);

	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
}

void MacroExportImportDialog::ExportMacros(const QString &json)
{
	MacroExportImportDialog dialog(
		MacroExportImportDialog::Type::EXPORT_MACRO);
	dialog._importExportString->setPlainText(json);
	dialog.adjustSize();
	dialog.updateGeometry();
	dialog.exec();
}

bool MacroExportImportDialog::ImportMacros(QString &json)
{
	MacroExportImportDialog dialog(
		MacroExportImportDialog::Type::IMPORT_MACRO);
	if (dialog.exec() == QDialog::Accepted) {
		json = dialog._importExportString->toPlainText();
		return true;
	}
	return false;
}

} // namespace advss
