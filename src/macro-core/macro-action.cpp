#include "macro-action.hpp"
#include "advanced-scene-switcher.hpp"

bool MacroAction::Save(obs_data_t *obj)
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

void MacroAction::LogAction()
{
	vblog(LOG_INFO, "performed action %s", GetId().c_str());
}

void MacroRefAction::ResolveMacroRef()
{
	_macro.UpdateRef();
}

void MultiMacroRefAction::ResolveMacroRef()
{
	for (auto &m : _macros) {
		m.UpdateRef();
	}
}
