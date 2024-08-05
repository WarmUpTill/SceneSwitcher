#include "macro-action-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

#include "properties-view.hpp"

// obs_properties_apply_settings -> apply current settings
// add signal callback to get settings, since they are automatically destroyed by the properties view

namespace advss {

static std::atomic_int completionIdCounter = 0;
static constexpr std::string_view completionIdParam = "completion_id";

MacroActionScript::MacroActionScript(
	Macro *m, const std::string &id,
	const std::shared_ptr<obs_properties_t> &properties,
	const OBSData &defaultSettings, bool blocking,
	const std::string &signal, const std::string &signalComplete)
	: MacroAction(m),
	  _id(id),
	  _properties(properties),
	  _settings(obs_data_create()),
	  _blocking(blocking),
	  _signal(signal),
	  _signalComplete(signalComplete)
{
	obs_data_apply(_settings.Get(), defaultSettings.Get());
	signal_handler_connect(obs_get_signal_handler(), signalComplete.c_str(),
			       &MacroActionScript::CompletionSignalReceived,
			       this);
}

MacroActionScript::MacroActionScript(const advss::MacroActionScript &other)
	: MacroAction(other.GetMacro()),
	  _id(other._id),
	  _properties(other._properties),
	  _settings(obs_data_create()),
	  _blocking(other._blocking),
	  _signal(other._signal),
	  _signalComplete(other._signalComplete)
{
	obs_data_apply(_settings.Get(), other._settings.Get());
}

void MacroActionScript::UpdateSettings(obs_data_t *newSettings) const
{
	obs_data_clear(_settings);
	obs_data_apply(_settings, newSettings);
}

void MacroActionScript::CompletionSignalReceived(void *param, calldata_t *data)
{
	auto action = static_cast<MacroActionScript *>(param);
	long long int id;
	if (!calldata_get_int(data, completionIdParam.data(), &id)) {
		blog(LOG_WARNING,
		     "received completion signal without \"%s\" parameter",
		     completionIdParam.data());
		return;
	}
	if (id != action->_completionId) {
		return;
	}
	action->_actionIsComplete = true;
}

void MacroActionScript::WaitForActionCompletion() const
{
	using namespace std::chrono_literals;
	auto start = std::chrono::high_resolution_clock::now();
	auto timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(
		start - start);
	const auto timeoutMs = _timeout.Seconds() * 1000.0;

	std::unique_lock<std::mutex> lock(*GetMutex());
	while (!_actionIsComplete) {
		if (MacroWaitShouldAbort() || MacroIsStopped(GetMacro())) {
			break;
		}

		if ((double)timePassed.count() > timeoutMs) {
			blog(LOG_INFO, "script action timeout (%s)",
			     _id.c_str());
			break;
		}

		GetMacroWaitCV().wait_for(lock, 10ms);
		const auto now = std::chrono::high_resolution_clock::now();
		timePassed =
			std::chrono::duration_cast<std::chrono::milliseconds>(
				now - start);
	}
}

bool MacroActionScript::PerformAction()
{
	_completionId = ++completionIdCounter;
	_actionIsComplete = false;

	auto data = calldata_create();
	calldata_set_string(data, GetActionCompletionSignalParamName().data(),
			    _signalComplete.c_str());
	calldata_set_int(data, completionIdParam.data(), _completionId);
	calldata_set_string(data, "settings", obs_data_get_json(_settings));
	signal_handler_signal(obs_get_signal_handler(), _signal.c_str(), data);
	calldata_destroy(data);

	if (_blocking) {
		SetMacroAbortWait(false);
		WaitForActionCompletion();
	}
	return true;
}

void MacroActionScript::LogAction() const
{
	ablog(LOG_INFO, "performing script action \"%s\"", _id.c_str());
}

bool MacroActionScript::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	obs_data_set_obj(obj, "settings", _settings.Get());
	_timeout.Save(obj);
	return true;
}

bool MacroActionScript::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	OBSDataAutoRelease settings = obs_data_get_obj(obj, "settings");
	obs_data_apply(_settings.Get(), settings);
	_timeout.Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionScript::Copy() const
{
	return std::make_shared<MacroActionScript>(*this);
}

static auto test = obs_data_create();

obs_properties_t *MacroActionScriptEdit::GetProperties(void *obj)
{
	auto actionEdit = static_cast<MacroActionScriptEdit *>(obj);
	return actionEdit->_entryData->GetProperties();
}

void MacroActionScriptEdit::UpdateSettings(void *obj, obs_data_t *settings)
{
	auto actionEdit = static_cast<MacroActionScriptEdit *>(obj);

	if (actionEdit->_loading || !actionEdit->_entryData) {
		return;
	}
	auto lock = LockContext();
	actionEdit->_entryData->UpdateSettings(settings);
}

MacroActionScriptEdit::MacroActionScriptEdit(
	QWidget *parent, std::shared_ptr<MacroActionScript> entryData)
	: QWidget(parent),
	  _timeout(new DurationSelection(this))
{
	QWidget::connect(_timeout, &DurationSelection::DurationChanged, this,
			 &MacroActionScriptEdit::TimeoutChanged);

	auto timeoutLayout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.script.timeout"),
		     timeoutLayout, {{"{{timeout}}", _timeout}});

	auto layout = new QVBoxLayout();
	OBSPropertiesView *view = new OBSPropertiesView(
		test, this, GetProperties, nullptr, UpdateSettings);
	layout->addWidget(view);
	layout->addLayout(timeoutLayout);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionScriptEdit::UpdateEntryData()
{
	_timeout->SetDuration(_entryData->_timeout);
}

void MacroActionScriptEdit::TimeoutChanged(const Duration &timeout)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_timeout = timeout;
}

} // namespace advss
