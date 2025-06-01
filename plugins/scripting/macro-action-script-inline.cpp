#include "macro-action-script-inline.hpp"
#include "layout-helpers.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"

#include <obs-module.h>
#include <QFileInfo>
#include <QDir>

namespace advss {

const std::string MacroActionScriptInline::_id = "script";

bool MacroActionScriptInline::_registered = MacroActionFactory::Register(
	MacroActionScriptInline::_id,
	{MacroActionScriptInline::Create, MacroActionScriptInlineEdit::Create,
	 "AdvSceneSwitcher.action.script"});

MacroActionScriptInline::MacroActionScriptInline(
	const MacroActionScriptInline &other)
	: MacroAction(other.GetMacro()),
	  _language(other._language),
	  _scriptText(other._scriptText)
{
	SetupScript();
}

bool MacroActionScriptInline::PerformAction()
{
	if (_lastResolvedText != std::string(_scriptText)) {
		SetupScript();
	}

	//if (!ScriptHandler::ActionIdIsValid(_id)) {
	//	blog(LOG_WARNING, "skipping unknown script action \"%s\"",
	//	     _id.c_str());
	//	return true;
	//}
	//
	//(void)SendTriggerSignal();
	return true;
}

void MacroActionScriptInline::LogAction() const
{
	ablog(LOG_INFO, "performing inline script action");
}

bool MacroActionScriptInline::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_int(obj, "language", _language);
	_scriptText.Save(obj, "script");
	return true;
}

bool MacroActionScriptInline::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_language =
		static_cast<obs_script_lang>(obs_data_get_int(obj, "language"));
	_scriptText.Load(obj, "script");
	SetupScript();
	return true;
}

std::shared_ptr<MacroAction> MacroActionScriptInline::Create(Macro *m)
{
	return std::make_shared<MacroActionScriptInline>(m);
}

std::shared_ptr<MacroAction> MacroActionScriptInline::Copy() const
{
	return std::make_shared<MacroActionScriptInline>(*this);
}

void MacroActionScriptInline::ResolveVariablesToFixedValues()
{
	_scriptText.ResolveVariables();
}

void MacroActionScriptInline::SetLanguage(obs_script_lang language)
{
	_language = language;
	SetupScript();
}

void MacroActionScriptInline::SetScript(const QString &text)
{
	_scriptText = text.toStdString();
	SetupScript();
}

static std::optional<std::string> createScriptTempFile(const char *content,
						       obs_script_lang language)
{
	static int counter = 0;
	++counter;
	static const QString filenamePattern =
		"scripting/advss-tmp-script%1.%2";
	const QString filename = filenamePattern.arg(counter).arg(
		language == OBS_SCRIPT_LANG_PYTHON ? "py" : "lua");
	auto settingsFile =
		obs_module_config_path(filename.toStdString().c_str());
	if (!settingsFile) {
		blog(LOG_WARNING,
		     "could not create script file! (obs_module_config_path)");
		return {};
	}

	const QString dirPath = QFileInfo(settingsFile).absolutePath();
	const QDir dir(dirPath);
	if (!dir.exists() && !dir.mkpath(dirPath)) {
		blog(LOG_WARNING, "could not create script file! (mkpath)");
		bfree(settingsFile);
		return {};
	}

	QFile file(settingsFile);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		bfree(settingsFile);
		return {};
	}

	auto out = QTextStream(&file);
	out << content;
	std::string path = settingsFile;
	bfree(settingsFile);
	return path;
}

void MacroActionScriptInline::SetupScript()
{

	// TODO: pre-process input with helper funcs
	// obs_get_module_data_path(obs_current_module()) +
	//		  std::string("/res/ocr");

	auto path = createScriptTempFile(_scriptText.c_str(), _language);
	if (!path) {
		_lastResolvedText = "";
		return;
	}
	_script = std::unique_ptr<obs_script_t, ScriptDeleter>(
		obs_script_create(path->c_str(), nullptr), {*path});
	_lastResolvedText = _scriptText;
}

void MacroActionScriptInline::CleanupScriptFile(const std::string &path)
{
	const QFileInfo fileInfo(QString::fromStdString(path));
	if (!fileInfo.isFile()) {
		return;
	}
	QFile file(fileInfo.absoluteFilePath());
	if (!file.remove()) {
		vblog(LOG_INFO, "failed to clean up script file %s",
		      fileInfo.absoluteFilePath().toStdString().c_str());
	}
}

void MacroActionScriptInline::WaitForCompletion() const
{
	//using namespace std::chrono_literals;
	//auto start = std::chrono::high_resolution_clock::now();
	//auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
	//	start - start);
	//const auto timeoutMs = GetTimeoutSeconds() * 1000.0;
	//
	//std::unique_lock<std::mutex> lock(*GetMutex());
	//while (!TriggerIsCompleted()) {
	//	if (MacroWaitShouldAbort() || MacroIsStopped(GetMacro())) {
	//		break;
	//	}
	//
	//	if ((double)timePassed.count() > timeoutMs) {
	//		blog(LOG_INFO, "script action timeout (%s)",
	//		     _id.c_str());
	//		break;
	//	}
	//
	//	GetMacroWaitCV().wait_for(lock, 10ms);
	//	const auto now = std::chrono::high_resolution_clock::now();
	//	timePassed =
	//		std::chrono::duration_cast<std::chrono::milliseconds>(
	//			now - start);
	//}
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

MacroActionScriptInlineEdit::MacroActionScriptInlineEdit(
	QWidget *parent, std::shared_ptr<MacroActionScriptInline> entryData)
	: QWidget(parent),
	  _language(new QComboBox(this)),
	  _script(new VariableTextEdit(this)),
	  _entryData(entryData)
{
	SetupLayout();
	SetupWidgetConnections();
	PopulateWidgets();
	SetWidgetVisibility();
	_loading = false;
}

void MacroActionScriptInlineEdit::LanguageChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetLanguage(
		static_cast<obs_script_lang>(_language->itemData(idx).toInt()));
}

void MacroActionScriptInlineEdit::ScriptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetScript(_script->toPlainText());
}

void MacroActionScriptInlineEdit::PopulateWidgets()
{
	populateLanguageSelection(_language);

	if (!_entryData) {
		return;
	}

	_language->setCurrentIndex(
		_language->findData(_entryData->GetLanguage()));
	_script->setPlainText(_entryData->GetScript());
}

void MacroActionScriptInlineEdit::SetupWidgetConnections()
{
	QWidget::connect(_language, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(LanguageChanged(int)));
	QWidget::connect(_script, SIGNAL(textChanged()), this,
			 SLOT(ScriptChanged()));
}

void MacroActionScriptInlineEdit::SetupLayout()
{
	auto languageLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.script.language.layout"),
		     languageLayout, {{"{{language}}", _language}});
	auto layout = new QVBoxLayout();
	layout->addLayout(languageLayout);
	layout->addWidget(_script);
	setLayout(layout);
}

QWidget *
MacroActionScriptInlineEdit::Create(QWidget *parent,
				    std::shared_ptr<MacroAction> action)
{
	return new MacroActionScriptInlineEdit(
		parent,
		std::dynamic_pointer_cast<MacroActionScriptInline>(action));
}

void MacroActionScriptInlineEdit::SetWidgetVisibility()
{
	// TODO
}

} // namespace advss
