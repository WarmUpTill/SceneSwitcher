#include "macro-condition-script-inline.hpp"

namespace advss {

const std::string MacroConditionScriptInline::_id = "script";

bool MacroConditionScriptInline::_registered =
	MacroConditionFactory::Register(MacroConditionScriptInline::_id,
					{MacroConditionScriptInline::Create,
					 MacroConditionScriptInlineEdit::Create,
					 "AdvSceneSwitcher.condition.script"});

bool MacroConditionScriptInline::CheckCondition()
{
	return _script.Run();
}

bool MacroConditionScriptInline::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_script.Save(obj);
	return true;
}

bool MacroConditionScriptInline::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_script.Load(obj);
	return true;
}

std::shared_ptr<MacroCondition> MacroConditionScriptInline::Create(Macro *m)
{
	return std::make_shared<MacroConditionScriptInline>(m);
}

MacroConditionScriptInlineEdit::MacroConditionScriptInlineEdit(
	QWidget *parent, std::shared_ptr<MacroConditionScriptInline> entryData)
	: MacroSegmentScriptInlineEdit(parent, entryData)
{
}

QWidget *MacroConditionScriptInlineEdit::Create(
	QWidget *parent, std::shared_ptr<MacroCondition> condition)
{
	return new MacroConditionScriptInlineEdit(
		parent, std::dynamic_pointer_cast<MacroConditionScriptInline>(
				condition));
}

} // namespace advss
