#include "macro-condition-script.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

namespace advss {

MacroConditionScript::MacroConditionScript(Macro *m, const std::string &id,
					   const std::string &signal)
	: MacroCondition(m),
	  _id(id),
	  _signal(signal)
{
}

MacroConditionScript::MacroConditionScript(
	const advss::MacroConditionScript &other)
	: MacroCondition(other.GetMacro()),
	  _id(other._id),
	  _signal(other._signal)
{
}

void MacroConditionScript::SignalReceived(void *param, calldata_t *)
{
	auto condition = static_cast<MacroConditionScript *>(param);
	condition->_conditionValue = true;
}

bool MacroConditionScript::CheckCondition()
{
	//	_actionIsComplete = false;
	//
	//	auto data = calldata_create();
	//	calldata_set_string(data, GetCompletionSignalParamName().data(),
	//			    _signalComplete.c_str());
	//	signal_handler_signal(obs_get_signal_handler(), _signal.c_str(), data);
	//	calldata_destroy(data);
	//
	//	if (_blocking) {
	//		SetMacroAbortWait(false);
	//		WaitForConditionCompletion();
	//	}
	return true;
}

bool MacroConditionScript::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	return true;
}

bool MacroConditionScript::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	return true;
}

MacroConditionScriptEdit::MacroConditionScriptEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScript> entryData)
	: QWidget(parent)
{
	auto layout = new QHBoxLayout();
	layout->addWidget(new QLabel("dummy"));
	setLayout(layout);

	_entryData = entryData;
	_loading = false;
}

} // namespace advss
