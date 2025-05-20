#include "macro-condition-game-capture.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroConditionGameCapture::id = "game_capture";

#ifdef _WIN32
bool MacroConditionGameCapture::_registered = MacroConditionFactory::Register(
	MacroConditionGameCapture::id,
	{MacroConditionGameCapture::Create,
	 MacroConditionGameCaptureEdit::Create,
	 "AdvSceneSwitcher.condition.gameCapture"});
#else
bool MacroConditionGameCapture::_registered =
	false; // For now the 'game_capture' source only exists on Windows
#endif

bool MacroConditionGameCapture::CheckCondition()
{
	auto source = OBSGetStrongRef(_source.GetSource());
	if (source != _lastSource) {
		SetupSignalHandler(source);
		return false;
	}

	std::lock_guard<std::mutex> lock(_mtx);
	if (_hooked) {
		SetTempVarValue("title", _title);
		SetTempVarValue("class", _class);
		SetTempVarValue("executable", _executable);
	}

	return _hooked;
}

bool MacroConditionGameCapture::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	return true;
}

bool MacroConditionGameCapture::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	SetupSignalHandler(OBSGetStrongRef(_source.GetSource()));
	return true;
}

void MacroConditionGameCapture::GetCalldataInfo(calldata_t *cd)
{
	const char *title = "";
	if (!calldata_get_string(cd, "title", &title)) {
		blog(LOG_WARNING, "%s failed to get title", __func__);
	}
	_title = title;
	const char *className = "";
	if (!calldata_get_string(cd, "class", &className)) {
		blog(LOG_WARNING, "%s failed to get class", __func__);
	}
	_class = className;
	const char *executable = "";
	if (!calldata_get_string(cd, "executable", &executable)) {
		blog(LOG_WARNING, "%s failed to get executable", __func__);
	}
	_executable = executable;
}

void MacroConditionGameCapture::HookedSignalReceived(void *data, calldata_t *cd)
{
	auto condition = static_cast<MacroConditionGameCapture *>(data);
	std::lock_guard<std::mutex> lock(condition->_mtx);
	condition->GetCalldataInfo(cd);
	condition->_hooked = true;
}

void MacroConditionGameCapture::UnhookedSignalReceived(void *data, calldata_t *)
{
	auto condition = static_cast<MacroConditionGameCapture *>(data);
	std::lock_guard<std::mutex> lock(condition->_mtx);
	condition->_hooked = false;
}

void MacroConditionGameCapture::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	AddTempvar(
		"title",
		obs_module_text("AdvSceneSwitcher.tempVar.gameCapture.title"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.gameCapture.title.description"));
	AddTempvar(
		"class",
		obs_module_text("AdvSceneSwitcher.tempVar.gameCapture.class"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.gameCapture.class.description"));
	AddTempvar(
		"executable",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.gameCapture.executable"),
		obs_module_text(
			"AdvSceneSwitcher.tempVar.gameCapture.executable.description"));
}

void MacroConditionGameCapture::SetupSignalHandler(obs_source_t *source)
{
	signal_handler_t *sh = obs_source_get_signal_handler(source);
	_hookSignal = OBSSignal(sh, "hooked", HookedSignalReceived, this);
	_unhookSignal = OBSSignal(sh, "unhooked", UnhookedSignalReceived, this);
	_lastSource = source;

	SetupInitialState(source);
}

void MacroConditionGameCapture::SetupInitialState(obs_source_t *source)
{
	std::lock_guard<std::mutex> lock(_mtx);

	_hooked = false;
	if (!source) {
		return;
	}

	calldata_t cd;
	calldata_init(&cd);
	auto ph = obs_source_get_proc_handler(source);
	if (!proc_handler_call(ph, "get_hooked", &cd)) {
		blog(LOG_WARNING,
		     "%s failed to call proc_handler for 'get_hooked'",
		     __func__);
		return;
	}

	if (!calldata_get_bool(&cd, "hooked", &_hooked)) {
		blog(LOG_WARNING, "%s failed to get hooked state", __func__);
	}
	GetCalldataInfo(&cd);

	calldata_free(&cd);
}

static QStringList getGameCaptureSourcesList()
{
	auto sourceEnum = [](void *param, obs_source_t *source) -> bool /* -- */
	{
		QStringList *list = reinterpret_cast<QStringList *>(param);

		if (strcmp(obs_source_get_unversioned_id(source),
			   "game_capture") == 0) {
			*list << obs_source_get_name(source);
		}
		return true;
	};

	QStringList list;
	obs_enum_sources(sourceEnum, &list);
	list.sort();
	return list;
}

MacroConditionGameCaptureEdit::MacroConditionGameCaptureEdit(
	QWidget *parent, std::shared_ptr<MacroConditionGameCapture> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, getGameCaptureSourcesList,
					     true))
{
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));

	auto layout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.gameCapture.entry"),
		layout, {{"{{sources}}", _sources}});
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionGameCaptureEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_source = source;
}

void MacroConditionGameCaptureEdit::UpdateEntryData()
{
	_sources->SetSource(_entryData->_source);
}

} // namespace advss
