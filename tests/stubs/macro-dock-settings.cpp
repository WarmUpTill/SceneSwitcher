#include "macro-dock-settings.hpp"

namespace advss {

MacroDockSettings::MacroDockSettings(Macro *macro) : _macro(macro) {}
MacroDockSettings::~MacroDockSettings() {}

void MacroDockSettings::Save(obs_data_t *, bool) const {}
void MacroDockSettings::Load(obs_data_t *) {}
void MacroDockSettings::EnableDock(bool value)
{
	_registerDock = value;
}
void MacroDockSettings::SetIsStandaloneDock(bool value)
{
	_standaloneDock = value;
}
void MacroDockSettings::SetDockWindowName(const std::string &name)
{
	_dockWindow = name;
}
void MacroDockSettings::SetHasRunButton(bool value)
{
	_hasRunButton = value;
}
void MacroDockSettings::SetHasPauseButton(bool value)
{
	_hasPauseButton = value;
}
void MacroDockSettings::SetHasStatusLabel(bool value)
{
	_hasStatusLabel = value;
}
void MacroDockSettings::SetHighlightEnable(bool value)
{
	_highlight = value;
}
void MacroDockSettings::SetRunButtonText(const std::string &text)
{
	_runButtonText = text;
}
void MacroDockSettings::SetPauseButtonText(const std::string &text)
{
	_pauseButtonText = text;
}
void MacroDockSettings::SetUnpauseButtonText(const std::string &text)
{
	_unpauseButtonText = text;
}
void MacroDockSettings::SetConditionsTrueStatusText(const std::string &text)
{
	_conditionsTrueStatusText = text;
}
void MacroDockSettings::SetConditionsFalseStatusText(const std::string &text)
{
	_conditionsFalseStatusText = text;
}

StringVariable MacroDockSettings::ConditionsTrueStatusText() const
{
	return _conditionsTrueStatusText;
}
StringVariable MacroDockSettings::ConditionsFalseStatusText() const
{
	return _conditionsFalseStatusText;
}

void MacroDockSettings::HandleMacroNameChange() {}
void MacroDockSettings::ResetDockIfEnabled() {}
void MacroDockSettings::RemoveDock() {}

std::string MacroDockSettings::GenerateId()
{
	return "";
}

} // namespace advss
