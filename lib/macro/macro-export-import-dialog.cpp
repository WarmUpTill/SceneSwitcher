#include "macro-export-import-dialog.hpp"
#include "macro-export-extensions.hpp"
#include "obs-module-helper.hpp"

#include <obs.hpp>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLayout>
#include <algorithm>
#include <numeric>

namespace advss {

static bool usePlainText = false;

// ---------------------------------------------------------------------------
// Compress / decompress helpers
// ---------------------------------------------------------------------------

static QString compressMacroString(const QString &input)
{
	QByteArray inputData = input.toUtf8();
	auto compressedData = qCompress(inputData);
	QByteArray encodedData = compressedData.toBase64();
	return QString::fromUtf8(encodedData);
}

static QString decompressMacroString(const QString &input)
{
	QByteArray encodedData = input.toUtf8();
	QByteArray compressedData = QByteArray::fromBase64(encodedData);
	auto outputData = qUncompress(compressedData);
	return QString::fromUtf8(outputData);
}

static bool isValidData(const QString &json)
{
	OBSDataAutoRelease data =
		obs_data_create_from_json(json.toStdString().c_str());
	return !!data;
}

// ---------------------------------------------------------------------------
// Extension section
// ---------------------------------------------------------------------------

Section *MacroExportImportDialog::BuildExtensionSection()
{
	auto outerSection = new Section(300, this);
	outerSection->AddHeaderWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.macroTab.export.additionalContent")));

	auto outerContent = new QWidget();
	auto outerLayout = new QVBoxLayout(outerContent);
	outerLayout->setContentsMargins(4, 4, 4, 4);
	outerLayout->setSpacing(4);

	const auto &extensions = GetMacroExportExtensions();

	// Build a sorted index list (alphabetical by translated display name).
	std::vector<int> order(extensions.size());
	std::iota(order.begin(), order.end(), 0);
	std::sort(order.begin(), order.end(), [&](int a, int b) {
		return QString::localeAwareCompare(
			       obs_module_text(extensions[a].displayNameKey),
			       obs_module_text(extensions[b].displayNameKey)) <
		       0;
	});

	for (int extIdx : order) {
		const auto &ext = extensions[extIdx];
		ExtensionUI ui;

		// Simple all-or-nothing checkbox (no sub-items).
		if (!ext.getExportItems) {
			ui.mainCheck = new QCheckBox(
				obs_module_text(ext.displayNameKey),
				outerContent);
			ui.mainCheck->setChecked(false);
			connect(ui.mainCheck, &QCheckBox::stateChanged, this,
				&MacroExportImportDialog::UpdateExportString);
			outerLayout->addWidget(ui.mainCheck);
			_extensionUIs.append(std::move(ui));
			continue;
		}

		// Extensions with per-item selection get their own inner
		// Section: the main checkbox sits in the header (next to the
		// toggle arrow) and the per-item checkboxes live in the
		// collapsible content area.
		const auto items = ext.getExportItems();
		if (items.isEmpty()) {
			continue;
		}

		auto innerSection = new Section(200, outerContent);

		ui.mainCheck =
			new QCheckBox(obs_module_text(ext.displayNameKey));
		ui.mainCheck->setChecked(false);
		innerSection->AddHeaderWidget(ui.mainCheck);

		auto subWidget = new QWidget();
		auto subLayout = new QVBoxLayout(subWidget);
		subLayout->setContentsMargins(4, 2, 4, 2);
		subLayout->setSpacing(2);

		for (const auto &[id, displayName] : items) {
			auto cb = new QCheckBox(displayName, subWidget);
			cb->setChecked(false);
			subLayout->addWidget(cb);
			ui.itemChecks.append({id, cb});
			connect(cb, &QCheckBox::stateChanged, this,
				&MacroExportImportDialog::UpdateExportString);
		}

		// Main checkbox toggles all sub-items.
		// Capture uiIdx (position in _extensionUIs after append).
		const int uiIdx = _extensionUIs.size();
		connect(ui.mainCheck, &QCheckBox::stateChanged, this,
			[this, uiIdx](int state) {
				if (state == Qt::PartiallyChecked)
					return;
				const bool checked = (state == Qt::Checked);
				if (uiIdx < _extensionUIs.size()) {
					for (auto &[id, cb] :
					     _extensionUIs[uiIdx].itemChecks) {
						QSignalBlocker b(cb);
						cb->setChecked(checked);
					}
				}
				UpdateExportString();
			});

		innerSection->SetContent(subWidget, false);
		outerLayout->addWidget(innerSection);
		_extensionUIs.append(std::move(ui));
	}

	outerSection->SetContent(outerContent, true);
	return outerSection;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

MacroExportImportDialog::MacroExportImportDialog(Type type,
						 const QString &baseJson)
	: QDialog(nullptr),
	  _baseJson(baseJson),
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

	auto layout = new QVBoxLayout(this);

	if (type == Type::EXPORT_MACRO && !GetMacroExportExtensions().empty()) {
		layout->addWidget(BuildExtensionSection());
	}

	layout->addWidget(label);
	layout->addWidget(_importExportString);
	layout->addWidget(_usePlainText);
	layout->addWidget(buttons);
	setLayout(layout);

	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));

	if (type == Type::EXPORT_MACRO) {
		RefreshExportText();
	}
}

// ---------------------------------------------------------------------------
// Export string helpers
// ---------------------------------------------------------------------------

QString MacroExportImportDialog::BuildExportJson() const
{
	OBSDataAutoRelease data =
		obs_data_create_from_json(_baseJson.toStdString().c_str());
	if (!data) {
		return _baseJson;
	}

	const auto &extensions = GetMacroExportExtensions();
	for (int i = 0;
	     i < (int)extensions.size() && i < (int)_extensionUIs.size(); ++i) {
		if (!_extensionUIs[i].mainCheck->isChecked()) {
			continue;
		}

		QStringList selectedIds;
		for (const auto &[id, cb] : _extensionUIs[i].itemChecks) {
			if (cb->isChecked()) {
				selectedIds << id;
			}
		}
		extensions[i].save(data, selectedIds);
	}

	const char *json = obs_data_get_json(data);
	return json ? QString::fromUtf8(json) : QString{};
}

void MacroExportImportDialog::RefreshExportText()
{
	const QString json = BuildExportJson();
	if (usePlainText) {
		_importExportString->setPlainText(json);
	} else {
		_importExportString->setPlainText(compressMacroString(json));
	}
}

void MacroExportImportDialog::UpdateExportString()
{
	RefreshExportText();
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

// ---------------------------------------------------------------------------
// Static entry points
// ---------------------------------------------------------------------------

void MacroExportImportDialog::ExportMacros(obs_data_t *macroData)
{
	const char *rawJson = obs_data_get_json(macroData);
	const QString baseJson = rawJson ? QString::fromUtf8(rawJson)
					 : QString{};

	MacroExportImportDialog dialog(Type::EXPORT_MACRO, baseJson);
	dialog.adjustSize();
	dialog.updateGeometry();
	dialog.exec();
}

bool MacroExportImportDialog::ImportMacros(QString &json)
{
	MacroExportImportDialog dialog(Type::IMPORT_MACRO);
	if (dialog.exec() != QDialog::Accepted) {
		return false;
	}

	json = decompressMacroString(dialog._importExportString->toPlainText());
	if (!isValidData(json)) {
		// Fallback: support raw (uncompressed) JSON pasted directly.
		json = dialog._importExportString->toPlainText();
	}

	// Invoke all extension load callbacks.
	OBSDataAutoRelease data =
		obs_data_create_from_json(json.toStdString().c_str());
	if (data) {
		for (const auto &ext : GetMacroExportExtensions()) {
			ext.load(data, {});
		}
	}

	return true;
}

} // namespace advss
