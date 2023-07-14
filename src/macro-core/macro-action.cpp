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
	obs_data_set_bool(obj, "enabled", _enabled);
	return true;
}

bool MacroAction::Load(obs_data_t *obj)
{
	MacroSegment::Load(obj);
	obs_data_set_default_bool(obj, "enabled", true);
	_enabled = obs_data_get_bool(obj, "enabled");
	return true;
}

void MacroAction::LogAction() const
{
	vblog(LOG_INFO, "performed action %s", GetId().c_str());
}

void MacroAction::SetEnabled(bool value)
{
	_enabled = value;
}

bool MacroAction::Enabled() const
{
	return _enabled;
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
