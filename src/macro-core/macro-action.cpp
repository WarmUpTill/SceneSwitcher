#include "macro-action.hpp"
#include "advanced-scene-switcher.hpp"

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

void MacroRefAction::ResolveMacroRef()
{
	_macro.UpdateRef();
}

MultiMacroRefAction::MultiMacroRefAction(Macro *m, bool supportsVariableValue)
	: MacroAction(m, supportsVariableValue)
{
}

void MultiMacroRefAction::ResolveMacroRef()
{
	for (auto &m : _macros) {
		m.UpdateRef();
	}
}
