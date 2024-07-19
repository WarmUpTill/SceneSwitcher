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
	signal_handler_connect(obs_get_signal_handler(), signal.c_str(),
			       &MacroConditionScript::ValueChangeSignalReceived,
			       this);
}

MacroConditionScript::MacroConditionScript(
	const advss::MacroConditionScript &other)
	: MacroCondition(other.GetMacro()),
	  _id(other._id),
	  _signal(other._signal)
{
}

void MacroConditionScript::ValueChangeSignalReceived(void *param,
						     calldata_t *data)
{
	auto condition = static_cast<MacroConditionScript *>(param);
	condition->_conditionValue = calldata_get_bool(
		data, GetConditionValueChangeSignalParamName().data());
}

bool MacroConditionScript::CheckCondition()
{
	return _conditionValue;
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
