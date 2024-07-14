#include "macro-action-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"

#include <obs.hpp>

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
	action->_actionComplete = true;
}

bool MacroActionScript::PerformAction()
{
	_actionComplete = false;
	auto data = calldata_create();
	calldata_set_string(data, GetCompletionSignalParamName().data(),
			    _signalComplete.c_str());
	signal_handler_signal(obs_get_signal_handler(), _signal.c_str(), data);
	calldata_destroy(data);
	if (_blocking) {
	}
	return true;
}

void MacroActionScript::LogAction() const
{
	// TODO
}

bool MacroActionScript::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	return true;
}

bool MacroActionScript::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	return true;
}

std::shared_ptr<MacroAction> MacroActionScript::Copy() const
{
	return std::make_shared<MacroActionScript>(*this);
}

MacroActionScriptEdit::MacroActionScriptEdit(
	QWidget *parent, std::shared_ptr<MacroActionScript> entryData)
	: QWidget(parent),
	  _test(new QPushButton("Test"))
{
	QWidget::connect(_test, &QPushButton::clicked, this,
			 &MacroActionScriptEdit::Test);

	auto layout = new QHBoxLayout();
	layout->addWidget(_test);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionScriptEdit::UpdateEntryData() {}

void MacroActionScriptEdit::Test()
{
	_entryData->PerformAction();
}

} // namespace advss
