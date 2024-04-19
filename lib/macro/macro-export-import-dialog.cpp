#include "macro-export-import-dialog.hpp"
#include "obs-module-helper.hpp"

#include <obs.hpp>
#include <QLayout>
#include <QLabel>
#include <QDialogButtonBox>

namespace advss {

static bool usePlainText = false;

MacroExportImportDialog::MacroExportImportDialog(Type type)
	: QDialog(nullptr),
	  _importExportString(new QPlainTextEdit(this)),
	  _usePlainText(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.macroTab.export.usePlainText")))
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

	_usePlainText->setChecked(usePlainText);
	_usePlainText->setVisible(type == Type::EXPORT_MACRO);
	connect(_usePlainText, &QCheckBox::stateChanged, this,
		&MacroExportImportDialog::UsePlainTextChanged);

	auto layout = new QVBoxLayout;
	layout->addWidget(label);
	layout->addWidget(_importExportString);
	layout->addWidget(_usePlainText);
	layout->addWidget(buttons);
	setLayout(layout);

	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
}

QString compressMacroString(const QString &input)
{
	QByteArray inputData = input.toUtf8();
	auto compressedData = qCompress(inputData);
	QByteArray encodedData = compressedData.toBase64();
	return QString::fromUtf8(encodedData);
}

QString decompressMacroString(const QString &input)
{
	QByteArray encodedData = input.toUtf8();
	QByteArray compressedData = QByteArray::fromBase64(encodedData);
	auto outputData = qUncompress(compressedData);
	return QString::fromUtf8(outputData);
}

void MacroExportImportDialog::ExportMacros(const QString &json)
{
	MacroExportImportDialog dialog(
		MacroExportImportDialog::Type::EXPORT_MACRO);
	dialog._importExportString->setPlainText(compressMacroString(json));
	dialog.adjustSize();
	dialog.updateGeometry();
	dialog.exec();
}

void MacroExportImportDialog::UsePlainTextChanged(int value)
{
	if (usePlainText == (!!value)) {
		return;
	}
	const auto current = _importExportString->toPlainText();
	if (usePlainText) {
		_importExportString->setPlainText(compressMacroString(current));
	} else {
		_importExportString->setPlainText(
			decompressMacroString(current));
	}
	usePlainText = value;
}

static bool isValidData(const QString &json)
{
	OBSDataAutoRelease data =
		obs_data_create_from_json(json.toStdString().c_str());
	return !!data;
}

bool MacroExportImportDialog::ImportMacros(QString &json)
{
	MacroExportImportDialog dialog(
		MacroExportImportDialog::Type::IMPORT_MACRO);
	if (dialog.exec() == QDialog::Accepted) {
		json = decompressMacroString(
			dialog._importExportString->toPlainText());
		if (!isValidData(json)) { // Fallback to support raw json format
			json = dialog._importExportString->toPlainText();
		}
		return true;
	}
	return false;
}

} // namespace advss
