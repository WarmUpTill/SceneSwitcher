#include "macro-condition-game-capture.hpp"
#include "layout-helpers.hpp"

namespace advss {

const std::string MacroConditionGameCapture::id = "game_capture";

bool MacroConditionGameCapture::_registered = MacroConditionFactory::Register(
	MacroConditionGameCapture::id,
	{MacroConditionGameCapture::Create,
	 MacroConditionGameCaptureEdit::Create,
	 "AdvSceneSwitcher.condition.gameCapture"});

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
	return true;
}

void MacroConditionGameCapture::HookedSignalReceived(void *data, calldata_t *cd)
{
	auto condition = static_cast<MacroConditionGameCapture *>(data);
	std::lock_guard<std::mutex> lock(condition->_mtx);
	if (!calldata_get_string(cd, "title", &condition->_title)) {
		condition->_title = "";
	}
	if (!calldata_get_string(cd, "class", &condition->_class)) {
		condition->_class = "";
	}
	if (!calldata_get_string(cd, "executable", &condition->_executable)) {
		condition->_executable = "";
	}
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
	_hookSignal = OBSSignal(sh, "unhooked", UnhookedSignalReceived, this);
}

static QStringList getGameCaptureSourceNames()
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
	return list;
}

MacroConditionGameCaptureEdit::MacroConditionGameCaptureEdit(
	QWidget *parent, std::shared_ptr<MacroConditionGameCapture> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, QStringList(), true))
{
	auto sources = getGameCaptureSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

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
