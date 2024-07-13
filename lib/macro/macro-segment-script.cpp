#include "macro-segment-script.hpp"
#include "layout-helpers.hpp"
#include "macro-action.hpp"
#include "macro-condition.hpp"
#include "macro-helpers.hpp"
#include "macro-script-handler.hpp"
#include "obs-module-helper.hpp"
#include "properties-view.hpp"
#include "sync-helpers.hpp"

namespace advss {

static std::atomic_int completionIdCounter = 0;

MacroSegmentScript::MacroSegmentScript(obs_data_t *defaultSettings,
				       const std::string &propertiesSignalName,
				       const std::string &triggerSignal,
				       const std::string &completionSignal)
	: _settings(obs_data_get_defaults(defaultSettings)),
	  _propertiesSignal(propertiesSignalName),
	  _triggerSignal(triggerSignal),
	  _completionSignal(completionSignal)
{
	signal_handler_connect(obs_get_signal_handler(),
			       completionSignal.c_str(),
			       &MacroSegmentScript::CompletionSignalReceived,
			       this);
}

MacroSegmentScript::MacroSegmentScript(const MacroSegmentScript &other)
	: _settings(obs_data_create()),
	  _propertiesSignal(other._propertiesSignal),
	  _triggerSignal(other._triggerSignal),
	  _completionSignal(other._completionSignal)
{
	signal_handler_connect(obs_get_signal_handler(),
			       _completionSignal.c_str(),
			       &MacroSegmentScript::CompletionSignalReceived,
			       this);
	obs_data_apply(_settings.Get(), other._settings.Get());
}

bool MacroSegmentScript::Save(obs_data_t *obj) const
{
	obs_data_set_obj(obj, "settings", _settings.Get());
	_timeout.Save(obj);
	return true;
}

bool MacroSegmentScript::Load(obs_data_t *obj)
{
	OBSDataAutoRelease settings = obs_data_get_obj(obj, "settings");
	obs_data_apply(_settings.Get(), settings);
	_timeout.Load(obj);
	return true;
}

obs_properties_t *MacroSegmentScript::GetProperties() const
{
	auto data = calldata_create();
	signal_handler_signal(obs_get_signal_handler(),
			      _propertiesSignal.c_str(), data);
	obs_properties_t *properties = nullptr;
	if (!calldata_get_ptr(data, GetPropertiesSignalParamName().data(),
			      &properties)) {
		calldata_destroy(data);
		return nullptr;
	}
	calldata_destroy(data);
	return properties;
}

void MacroSegmentScript::UpdateSettings(obs_data_t *newSettings) const
{
	obs_data_clear(_settings.Get());
	obs_data_apply(_settings.Get(), newSettings);
}

bool MacroSegmentScript::SendTriggerSignal()
{
	_completionId = ++completionIdCounter;
	_triggerIsComplete = false;
	_triggerResult = false;

	auto data = calldata_create();
	calldata_set_string(data, GetActionCompletionSignalParamName().data(),
			    _completionSignal.c_str());
	calldata_set_int(data, GeCompletionIdParamName().data(), _completionId);
	calldata_set_string(data, "settings", obs_data_get_json(GetSettings()));
	signal_handler_signal(obs_get_signal_handler(), _triggerSignal.c_str(),
			      data);
	calldata_destroy(data);

	SetMacroAbortWait(false);
	WaitForCompletion();

	return _triggerResult;
}

void MacroSegmentScript::CompletionSignalReceived(void *param, calldata_t *data)
{
	auto segment = static_cast<MacroSegmentScript *>(param);
	long long int id;
	if (!calldata_get_int(data, GeCompletionIdParamName().data(), &id)) {
		blog(LOG_WARNING,
		     "received completion signal without \"%s\" parameter",
		     GeCompletionIdParamName().data());
		return;
	}
	bool result;
	if (!calldata_get_bool(data, GeResultSignalParamName().data(),
			       &result)) {
		blog(LOG_WARNING,
		     "received completion signal without \"%s\" parameter",
		     GeResultSignalParamName().data());
		return;
	}
	if (id != segment->_completionId) {
		return;
	}
	segment->_triggerIsComplete = true;
	segment->_triggerResult = result;
}

obs_properties_t *MacroSegmentScriptEdit::GetProperties(void *obj)
{
	auto segmentEdit = reinterpret_cast<MacroSegmentScriptEdit *>(obj);
	if (!segmentEdit) {
		return nullptr;
	}
	return segmentEdit->_entryData->GetProperties();
}

void MacroSegmentScriptEdit::UpdateSettings(void *obj, obs_data_t *settings)
{
	auto segmentEdit = reinterpret_cast<MacroSegmentScriptEdit *>(obj);
	if (!segmentEdit || segmentEdit->_loading || !segmentEdit->_entryData) {
		return;
	}
	auto lock = LockContext();
	segmentEdit->_entryData->UpdateSettings(settings);
}

MacroSegmentScriptEdit::MacroSegmentScriptEdit(
	QWidget *parent, std::shared_ptr<MacroSegmentScript> entryData)
	: QWidget(parent),
	  _timeout(new DurationSelection(this))
{
	QWidget::connect(_timeout, &DurationSelection::DurationChanged, this,
			 &MacroSegmentScriptEdit::TimeoutChanged);

	auto timeoutLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.script.timeout"),
		     timeoutLayout, {{"{{timeout}}", _timeout}});

	auto layout = new QVBoxLayout();

	auto properties = entryData->GetProperties();
	if (!!properties) {
		obs_properties_destroy(properties);

		// We need a separate OBSData object here as we can't risk
		// entryData->_settings being modified while it is currently used
		OBSDataAutoRelease data = obs_data_create();
		obs_data_apply(data, entryData->GetSettings());

#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(30, 0, 0)
		auto propertiesView =
			new OBSPropertiesView(data.Get(), this, GetProperties,
					      nullptr, UpdateSettings);
		layout->addWidget(propertiesView);
		connect(propertiesView, &OBSPropertiesView::PropertiesResized,
			this, [this]() {
				adjustSize();
				updateGeometry();
			});
#else
		layout->addWidget(new QLabel(
			"Displaying script properties not supported when compiled for OBS 29!"));
#endif
	}

	layout->addLayout(timeoutLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	adjustSize();
	updateGeometry();
}

void MacroSegmentScriptEdit::UpdateEntryData()
{
	_timeout->SetDuration(_entryData->_timeout);
}

QWidget *MacroSegmentScriptEdit::Create(QWidget *parent,
					std::shared_ptr<MacroAction> segment)
{
	return new MacroSegmentScriptEdit(
		parent, std::dynamic_pointer_cast<MacroSegmentScript>(segment));
}

QWidget *MacroSegmentScriptEdit::Create(QWidget *parent,
					std::shared_ptr<MacroCondition> segment)
{
	return new MacroSegmentScriptEdit(
		parent, std::dynamic_pointer_cast<MacroSegmentScript>(segment));
}

void MacroSegmentScriptEdit::TimeoutChanged(const Duration &timeout)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_timeout = timeout;
}

} // namespace advss
