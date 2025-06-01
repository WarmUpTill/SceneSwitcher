#include "macro-action-script-inline.hpp"

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

MacroActionScriptInlineEdit::MacroActionScriptInlineEdit(
	QWidget *parent, std::shared_ptr<MacroActionScriptInline> entryData)
	: MacroSegmentScriptInlineEdit(parent, entryData)
{
}

QWidget *
MacroActionScriptInlineEdit::Create(QWidget *parent,
				    std::shared_ptr<MacroAction> action)
{
	return new MacroActionScriptInlineEdit(
		parent,
		std::dynamic_pointer_cast<MacroActionScriptInline>(action));
}

} // namespace advss
