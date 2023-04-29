#include "macro-action.hpp"

namespace advss {

MacroAction::MacroAction(Macro *m, bool supportsVariableValue)
	: MacroSegment(m, supportsVariableValue)
{
}

bool MacroAction::Save(obs_data_t *obj) const
{
	MacroSegment::Save(obj);
	obs_data_set_string(obj, "id", GetId().c_str());
	return true;
}

bool MacroAction::Load(obs_data_t *obj)
{
	MacroSegment::Load(obj);
	return true;
}

void MacroAction::LogAction() const
{
	vblog(LOG_INFO, "performed action %s", GetId().c_str());
}

MacroRefAction::MacroRefAction(Macro *m, bool supportsVariableValue)
	: MacroAction(m, supportsVariableValue)
{
}

bool MacroRefAction::PostLoad()
{
	_macro.PostLoad();
	return true;
}

MultiMacroRefAction::MultiMacroRefAction(Macro *m, bool supportsVariableValue)
	: MacroAction(m, supportsVariableValue)
{
}

bool MultiMacroRefAction::PostLoad()
{
	for (auto &macro : _macros) {
		macro.PostLoad();
	}
	return true;
}

} // namespace advss
