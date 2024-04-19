#pragma once
#include <QCheckBox>
#include <QDialog>
#include <QPlainTextEdit>

namespace advss {

class MacroExportImportDialog : public QDialog {
	Q_OBJECT
public:
	enum class Type { EXPORT_MACRO, IMPORT_MACRO };
	MacroExportImportDialog(Type type);

	static void ExportMacros(const QString &json);
	static bool ImportMacros(QString &json);

private slots:
	void UsePlainTextChanged(int);

private:
	QPlainTextEdit *_importExportString;
	QCheckBox *_usePlainText;
};

} // namespace advss
