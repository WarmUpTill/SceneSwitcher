#include "macro-segment-script-inline.hpp"
#include "layout-helpers.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"
#include "ui-helpers.hpp"

#include <QDesktopServices>

namespace advss {

void MacroSegmentScriptInline::SetType(InlineScript::Type type)
{
	_script.SetType(type);
}

void MacroSegmentScriptInline::SetLanguage(obs_script_lang language)
{
	_script.SetLanguage(language);
}

void MacroSegmentScriptInline::SetScript(const std::string &text)
{
	_script.SetText(text);
}

void MacroSegmentScriptInline::SetPath(const std::string &path)
{
	_script.SetPath(path);
}

static void populateLanguageSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.script.language.python"),
		OBS_SCRIPT_LANG_PYTHON);
	list->addItem(obs_module_text("AdvSceneSwitcher.script.language.lua"),
		      OBS_SCRIPT_LANG_LUA);
	list->setPlaceholderText(
		obs_module_text("AdvSceneSwitcher.script.language.select"));
}

static void populateScriptTypeSelection(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.script.type.inline"),
		      InlineScript::INLINE);
	list->addItem(obs_module_text("AdvSceneSwitcher.script.type.file"),
		      InlineScript::FILE);
}

MacroSegmentScriptInlineEdit::MacroSegmentScriptInlineEdit(
	QWidget *parent, std::shared_ptr<MacroSegmentScriptInline> entryData)
	: QWidget(parent),
	  _scriptType(new QComboBox(this)),
	  _language(new QComboBox(this)),
	  _script(new ScriptEditor(this)),
	  _path(new FileSelection(FileSelection::Type::WRITE, this)),
	  _openFile(new QPushButton(
		  obs_module_text("AdvSceneSwitcher.script.file.open"), this)),
	  _fileLayout(new QHBoxLayout()),
	  _entryData(entryData)
{
	SetupLayout();
	SetupWidgetConnections();
	PopulateWidgets();
	SetWidgetVisibility();
	_loading = false;
}

void MacroSegmentScriptInlineEdit::ScriptTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetType(static_cast<InlineScript::Type>(
		_scriptType->itemData(idx).toInt()));
	SetWidgetVisibility();
}

void MacroSegmentScriptInlineEdit::LanguageChanged(int idx)
{
	{
		GUARD_LOADING_AND_LOCK();
		_entryData->SetLanguage(static_cast<obs_script_lang>(
			_language->itemData(idx).toInt()));
		const QSignalBlocker b(_script);
		_script->setPlainText(_entryData->GetScript());
	}

	if (_entryData->GetType() == InlineScript::Type::FILE) {
		PathChanged(QString::fromStdString(_entryData->GetPath()));
	}
}

void MacroSegmentScriptInlineEdit::ScriptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetScript(_script->toPlainText().toStdString());
	adjustSize();
	updateGeometry();
}

void MacroSegmentScriptInlineEdit::PathChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();

	if (path.isEmpty()) {
		_entryData->SetPath(path.toStdString());
		return;
	}

	// Script language will be detected by OBS based on file extension so
	// adjust it if necessary
	QString pathAdjusted = path;
	if (_entryData->GetLanguage() == OBS_SCRIPT_LANG_PYTHON &&
	    !path.endsWith(".py")) {
		if (path.endsWith(".lua")) {
			pathAdjusted.chop(4);
		}
		pathAdjusted += ".py";
	}
	if (_entryData->GetLanguage() == OBS_SCRIPT_LANG_LUA &&
	    !path.endsWith(".lua")) {
		if (path.endsWith(".py")) {
			pathAdjusted.chop(3);
		}
		pathAdjusted += ".lua";
	}

	const QSignalBlocker b(_path);
	_path->SetPath(pathAdjusted);
	_entryData->SetPath(pathAdjusted.toStdString());
}

void MacroSegmentScriptInlineEdit::PopulateWidgets()
{
	populateLanguageSelection(_language);
	populateScriptTypeSelection(_scriptType);

	if (!_entryData) {
		return;
	}

	_scriptType->setCurrentIndex(
		_scriptType->findData(_entryData->GetType()));
	_language->setCurrentIndex(
		_language->findData(_entryData->GetLanguage()));
	_script->setPlainText(_entryData->GetScript());
	_path->SetPath(_entryData->GetPath());
}

void MacroSegmentScriptInlineEdit::SetupWidgetConnections()
{
	QWidget::connect(_scriptType, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ScriptTypeChanged(int)));
	QWidget::connect(_language, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(LanguageChanged(int)));
	QWidget::connect(_script, SIGNAL(ScriptChanged()), this,
			 SLOT(ScriptChanged()));
	QWidget::connect(_path, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(PathChanged(const QString &)));
	QWidget::connect(_openFile, &QPushButton::clicked, this, [this] {
		QUrl fileUrl = QUrl::fromLocalFile(
			QString::fromStdString(_entryData->GetPath()));
		if (!QDesktopServices::openUrl(fileUrl)) {
			DisplayMessage(obs_module_text(
				"AdvSceneSwitcher.script.file.open.failed"));
		}
	});
}

void MacroSegmentScriptInlineEdit::SetupLayout()
{
	auto languageLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.script.language.layout"),
		     languageLayout, {{"{{language}}", _language}});
	auto typeLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.script.type.layout"),
		     typeLayout, {{"{{scriptType}}", _scriptType}});
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.script.file.layout"),
		     _fileLayout,
		     {{"{{path}}", _path}, {"{{open}}", _openFile}}, false);
	auto layout = new QVBoxLayout();
	layout->addLayout(typeLayout);
	layout->addLayout(languageLayout);
	layout->addLayout(_fileLayout);
	layout->addWidget(_script);
	setLayout(layout);
}

void MacroSegmentScriptInlineEdit::SetWidgetVisibility()
{
	_script->setVisible(_entryData->GetType() ==
			    InlineScript::Type::INLINE);
	SetLayoutVisible(_fileLayout,
			 _entryData->GetType() == InlineScript::Type::FILE);
	adjustSize();
	updateGeometry();
}

} // namespace advss
