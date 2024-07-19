#include "macro-action-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

namespace advss {

MacroActionScript::MacroActionScript(Macro *m, const std::string &id,
				     bool blocking, const std::string &signal,
				     const std::string &signalComplete)
	: MacroAction(m),
	  _id(id),
	  _blocking(blocking),
	  _signal(signal),
	  _signalComplete(signalComplete)
{
	signal_handler_connect(obs_get_signal_handler(), signalComplete.c_str(),
			       &MacroActionScript::CompletionSignalReceived,
			       this);
}

MacroActionScript::MacroActionScript(const advss::MacroActionScript &other)
	: MacroAction(other.GetMacro()),
	  _id(other._id),
	  _blocking(other._blocking),
	  _signal(other._signal),
	  _signalComplete(other._signalComplete)
{
}

void MacroActionScript::CompletionSignalReceived(void *param, calldata_t *)
{
	auto action = static_cast<MacroActionScript *>(param);
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
	_actionIsComplete = false;

	auto data = calldata_create();
	calldata_set_string(data, GetActionCompletionSignalParamName().data(),
			    _signalComplete.c_str());
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
	_timeout.Save(obj);
	return true;
}

bool MacroActionScript::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_timeout.Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionScript::Copy() const
{
	return std::make_shared<MacroActionScript>(*this);
}

MacroActionScriptEdit::MacroActionScriptEdit(
	QWidget *parent, std::shared_ptr<MacroActionScript> entryData)
	: QWidget(parent),
	  _timeout(new DurationSelection(this))
{
	QWidget::connect(_timeout, &DurationSelection::DurationChanged, this,
			 &MacroActionScriptEdit::TimeoutChanged);

	auto layout = new QHBoxLayout();
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.script.timeout"),
		     layout, {{"{{timeout}}", _timeout}});
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
