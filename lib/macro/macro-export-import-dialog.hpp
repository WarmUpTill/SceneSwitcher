#pragma once

#include "section.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QList>
#include <QPlainTextEdit>

#include <obs-data.h>

namespace advss {

class MacroExportImportDialog : public QDialog {
	Q_OBJECT
public:
	enum class Type { EXPORT_MACRO, IMPORT_MACRO };

	static void ExportMacros(obs_data_t *macroData);
	static bool ImportMacros(QString &json);

private slots:
	void UsePlainTextChanged(int);
	void UpdateExportString();

private:
	explicit MacroExportImportDialog(Type type,
					 const QString &baseJson = {});

	// Per-extension widgets
	struct ExtensionUI {
		QCheckBox *mainCheck = nullptr;
		// (itemId, checkbox) pairs - empty when no per-item selection.
		QList<QPair<QString, QCheckBox *>> itemChecks;
	};

	Section *BuildExtensionSection();
	QString BuildExportJson() const;
	void RefreshExportText();

	QString _baseJson;
	QPlainTextEdit *_importExportString;
	QCheckBox *_usePlainText;
	QList<ExtensionUI> _extensionUIs;
};

} // namespace advss
