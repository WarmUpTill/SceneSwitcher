#include "macro-edit.hpp"

namespace advss {

MacroEdit::MacroEdit(QWidget *parent, QStringList)
	: QWidget(parent),
	  ui(std::make_unique<Ui_MacroEdit>())
{
}

void MacroEdit::SetMacro(const std::shared_ptr<Macro> &m)
{
	_currentMacro = m;
}
std::shared_ptr<Macro> MacroEdit::GetMacro() const
{
	return _currentMacro;
}
void MacroEdit::ClearSegmentWidgetCacheFor(Macro *) const {}
void MacroEdit::SetControlsDisabled(bool) const {}
void MacroEdit::HighlightAction(int, QColor) const {}
void MacroEdit::HighlightElseAction(int, QColor) const {}
void MacroEdit::HighlightCondition(int, QColor) const {}
void MacroEdit::ResetConditionHighlights() {}
void MacroEdit::ResetActionHighlights() {}
void MacroEdit::SetActionData(Macro &) const {}
void MacroEdit::SetElseActionData(Macro &) const {}
void MacroEdit::SetConditionData(Macro &) const {}
void MacroEdit::SwapActions(Macro *, int, int) {}
void MacroEdit::SwapElseActions(Macro *, int, int) {}
void MacroEdit::SwapConditions(Macro *, int, int) {}
void MacroEdit::CopyMacroSegment() {}
void MacroEdit::PasteMacroSegment() {}
bool MacroEdit::IsEmpty() const
{
	return true;
}
void MacroEdit::ShowAllMacroSections() {}

void MacroEdit::on_conditionAdd_clicked() {}
void MacroEdit::on_conditionRemove_clicked() {}
void MacroEdit::on_conditionTop_clicked() {}
void MacroEdit::on_conditionUp_clicked() {}
void MacroEdit::on_conditionDown_clicked() {}
void MacroEdit::on_conditionBottom_clicked() {}
void MacroEdit::on_actionAdd_clicked() {}
void MacroEdit::on_actionRemove_clicked() {}
void MacroEdit::on_actionTop_clicked() {}
void MacroEdit::on_actionUp_clicked() {}
void MacroEdit::on_actionDown_clicked() {}
void MacroEdit::on_actionBottom_clicked() {}
void MacroEdit::on_toggleElseActions_clicked() const {}
void MacroEdit::on_elseActionAdd_clicked() {}
void MacroEdit::on_elseActionRemove_clicked() {}
void MacroEdit::on_elseActionTop_clicked() {}
void MacroEdit::on_elseActionUp_clicked() {}
void MacroEdit::on_elseActionDown_clicked() {}
void MacroEdit::on_elseActionBottom_clicked() {}
void MacroEdit::UpMacroSegmentHotkey() {}
void MacroEdit::DownMacroSegmentHotkey() {}
void MacroEdit::DeleteMacroSegmentHotkey() {}
void MacroEdit::ShowMacroActionsContextMenu(const QPoint &) {}
void MacroEdit::ShowMacroElseActionsContextMenu(const QPoint &) {}
void MacroEdit::ShowMacroConditionsContextMenu(const QPoint &) {}
void MacroEdit::ExpandAllActions() const {}
void MacroEdit::ExpandAllElseActions() const {}
void MacroEdit::ExpandAllConditions() const {}
void MacroEdit::CollapseAllActions() const {}
void MacroEdit::CollapseAllElseActions() const {}
void MacroEdit::CollapseAllConditions() const {}
void MacroEdit::MinimizeActions() const {}
void MacroEdit::MaximizeActions() const {}
void MacroEdit::MinimizeElseActions() const {}
void MacroEdit::MaximizeElseActions() const {}
void MacroEdit::MinimizeConditions() const {}
void MacroEdit::MaximizeConditions() const {}
void MacroEdit::SetElseActionsStateToHidden() const {}
void MacroEdit::SetElseActionsStateToVisible() const {}
void MacroEdit::MacroActionSelectionChanged(int) {}
void MacroEdit::MacroActionReorder(int, int) {}
void MacroEdit::AddMacroAction(Macro *, int, const std::string &, obs_data_t *)
{
}
void MacroEdit::AddMacroAction(int) {}
void MacroEdit::RemoveMacroAction(int) {}
void MacroEdit::MoveMacroActionUp(int) {}
void MacroEdit::MoveMacroActionDown(int) {}
void MacroEdit::MacroElseActionSelectionChanged(int) {}
void MacroEdit::MacroElseActionReorder(int, int) {}
void MacroEdit::AddMacroElseAction(Macro *, int, const std::string &,
				   obs_data_t *)
{
}
void MacroEdit::AddMacroElseAction(int) {}
void MacroEdit::RemoveMacroElseAction(int) {}
void MacroEdit::MoveMacroElseActionUp(int) {}
void MacroEdit::MoveMacroElseActionDown(int) {}
void MacroEdit::MacroConditionSelectionChanged(int) {}
void MacroEdit::MacroConditionReorder(int, int) {}
void MacroEdit::AddMacroCondition(int) {}
void MacroEdit::AddMacroCondition(Macro *, int, const std::string &,
				  obs_data_t *, Logic::Type)
{
}
void MacroEdit::RemoveMacroCondition(int) {}
void MacroEdit::MoveMacroConditionUp(int) {}
void MacroEdit::MoveMacroConditionDown(int) {}
void MacroEdit::HighlightControls() const {}

void MacroEdit::PopulateMacroActions(Macro &, uint32_t) {}
void MacroEdit::PopulateMacroElseActions(Macro &, uint32_t) {}
void MacroEdit::PopulateMacroConditions(Macro &, uint32_t) {}
void MacroEdit::SetupMacroSegmentSelection(MacroSection, int) {}
void MacroEdit::SetupContextMenu(const QPoint &,
				 const std::function<void(MacroEdit *, int)> &,
				 const std::function<void(MacroEdit *)> &,
				 const std::function<void(MacroEdit *)> &,
				 const std::function<void(MacroEdit *)> &,
				 const std::function<void(MacroEdit *)> &,
				 MacroSegmentList *)
{
}
void MacroEdit::RunSegmentHighlightChecks() {}
bool MacroEdit::ElseSectionIsVisible() const
{
	return false;
}

bool MacroEdit::eventFilter(QObject *, QEvent *)
{
	return false;
}

} // namespace advss
