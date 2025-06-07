#include "macro-action-script-inline.hpp"
#include "layout-helpers.hpp"
#include "obs-module-helper.hpp"
#include "sync-helpers.hpp"

namespace advss {

const std::string MacroActionScriptInline::_id = "script";

bool MacroActionScriptInline::_registered = MacroActionFactory::Register(
	MacroActionScriptInline::_id,
	{MacroActionScriptInline::Create, MacroActionScriptInlineEdit::Create,
	 "AdvSceneSwitcher.action.script"});

bool MacroActionScriptInline::PerformAction()
{
	return _script.Run();
}

void MacroActionScriptInline::LogAction() const
{
	ablog(LOG_INFO, "performing inline script action");
}

bool MacroActionScriptInline::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_script.Save(obj);
	return true;
}

bool MacroActionScriptInline::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_script.Load(obj);
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
	_script.ResolveVariablesToFixedValues();
}

void MacroActionScriptInline::SetLanguage(obs_script_lang language)
{
	_script.SetLanguage(language);
}

void MacroActionScriptInline::SetScript(const QString &text)
{
	_script.SetText(text.toStdString());
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
	  _script(new ScriptEditor(this)),
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
	const QSignalBlocker b(_script);
	_script->setPlainText(_entryData->GetScript());
}

void MacroActionScriptInlineEdit::ScriptChanged()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetScript(_script->toPlainText());
	adjustSize();
	updateGeometry();
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
	QWidget::connect(_script, SIGNAL(ScriptChanged()), this,
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
