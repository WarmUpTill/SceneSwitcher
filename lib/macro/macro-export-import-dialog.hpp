#pragma once
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

private:
	QPlainTextEdit *_importExportString;
};

} // namespace advss
