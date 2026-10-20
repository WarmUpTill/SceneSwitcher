#pragma once

#include "macro-export-prompts-dialog.hpp"

#include <QCheckBox>
#include <QDialog>
#include <QList>
#include <QPlainTextEdit>
#include <QPushButton>

#include <memory>
#include <obs-data.h>
#include <vector>

class QLayout;
class QScrollArea;
class QSplitter;

namespace advss {

class Macro;
struct MacroExportExtension;

class MacroExportImportDialog : public QDialog {
	Q_OBJECT
public:
	enum class Type { EXPORT_MACRO, IMPORT_MACRO };

	static void
	ExportMacros(obs_data_t *macroData,
		     const std::vector<std::shared_ptr<Macro>> &macros);
	static bool ImportMacros(QString &json);

private slots:
	void UsePlainTextChanged(int);
	void UpdateExportString();
	void EditImportPrompts();
	void FitExtensionSection();

private:
	explicit MacroExportImportDialog(
		Type type, const QString &baseJson = {},
		const std::vector<std::shared_ptr<Macro>> &macros = {});

	// Per-extension widgets
	struct ExtensionUI {
		QCheckBox *mainCheck = nullptr;
		// (itemId, checkbox) pairs - empty when no per-item selection.
		QList<QPair<QString, QCheckBox *>> itemChecks;
	};

	void AddExtensionCheckBox(const MacroExportExtension &ext, int extIdx,
				  QWidget *parent, QLayout *layout);
	void AddExtensionSection(const MacroExportExtension &ext, int extIdx,
				 QWidget *parent, QLayout *layout);
	void SetExtensionItemsChecked(int uiIdx, int state);
	void ExtensionItemChanged(int uiIdx);
	QWidget *BuildExtensionWidget();
	QString BuildExportJson() const;
	void RefreshExportText();

	QString _baseJson;
	std::vector<std::shared_ptr<Macro>> _macros;
	ExportPromptsState _promptsState;
	QPlainTextEdit *_importExportString;
	QCheckBox *_usePlainText;
	QPushButton *_editImportPrompts;
	QSplitter *_splitter = nullptr;
	QScrollArea *_extensionScrollArea = nullptr;
	QList<ExtensionUI> _extensionUIs;
	std::vector<int> _extensionOrder;
};

} // namespace advss
