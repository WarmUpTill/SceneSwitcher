#pragma once
#include "condition-logic.hpp"

#include <ui_macro-edit.h>

namespace advss {

class Macro;
class MacroSegment;

/*******************************************************************************
 * Advanced Scene Switcher window
 *******************************************************************************/
class MacroEdit : public QWidget {
	Q_OBJECT

public:
	MacroEdit(QWidget *parent, QStringList helpMsg = {});
	void SetMacro(const std::shared_ptr<Macro> &);
	void ClearSegmentWidgetCacheFor(Macro *) const;
	void SetControlsDisabled(bool disable) const;
	void HighlightAction(int idx, QColor color = QColor(Qt::green)) const;
	void HighlightElseAction(int idx,
				 QColor color = QColor(Qt::green)) const;
	void HighlightCondition(int idx,
				QColor color = QColor(Qt::green)) const;
	void ResetConditionHighlights();
	void ResetActionHighlights();
	void SetActionData(Macro &m) const;
	void SetElseActionData(Macro &m) const;
	void SetConditionData(Macro &m) const;
	void SwapActions(Macro *m, int pos1, int pos2);
	void SwapElseActions(Macro *m, int pos1, int pos2);
	void SwapConditions(Macro *m, int pos1, int pos2);
	void CopyMacroSegment();
	void PasteMacroSegment();
	bool IsEmpty() const;
	void ShowAllMacroSections();

private slots:
	void on_conditionAdd_clicked();
	void on_conditionRemove_clicked();
	void on_conditionTop_clicked();
	void on_conditionUp_clicked();
	void on_conditionDown_clicked();
	void on_conditionBottom_clicked();
	void on_actionAdd_clicked();
	void on_actionRemove_clicked();
	void on_actionTop_clicked();
	void on_actionUp_clicked();
	void on_actionDown_clicked();
	void on_actionBottom_clicked();
	void on_toggleElseActions_clicked() const;
	void on_elseActionAdd_clicked();
	void on_elseActionRemove_clicked();
	void on_elseActionTop_clicked();
	void on_elseActionUp_clicked();
	void on_elseActionDown_clicked();
	void on_elseActionBottom_clicked();
	void UpMacroSegmentHotkey();
	void DownMacroSegmentHotkey();
	void DeleteMacroSegmentHotkey();
	void ShowMacroActionsContextMenu(const QPoint &);
	void ShowMacroElseActionsContextMenu(const QPoint &);
	void ShowMacroConditionsContextMenu(const QPoint &);
	void ExpandAllActions() const;
	void ExpandAllElseActions() const;
	void ExpandAllConditions() const;
	void CollapseAllActions() const;
	void CollapseAllElseActions() const;
	void CollapseAllConditions() const;
	void MinimizeActions() const;
	void MaximizeActions() const;
	void MinimizeElseActions() const;
	void MaximizeElseActions() const;
	void MinimizeConditions() const;
	void MaximizeConditions() const;
	void SetElseActionsStateToHidden() const;
	void SetElseActionsStateToVisible() const;
	void MacroActionSelectionChanged(int idx);
	void MacroActionReorder(int to, int target);
	void AddMacroAction(Macro *macro, int idx, const std::string &id,
			    obs_data_t *data);
	void AddMacroAction(int idx);
	void RemoveMacroAction(int idx);
	void MoveMacroActionUp(int idx);
	void MoveMacroActionDown(int idx);
	void MacroElseActionSelectionChanged(int idx);
	void MacroElseActionReorder(int to, int target);
	void AddMacroElseAction(Macro *macro, int idx, const std::string &id,
				obs_data_t *data);
	void AddMacroElseAction(int idx);
	void RemoveMacroElseAction(int idx);
	void MoveMacroElseActionUp(int idx);
	void MoveMacroElseActionDown(int idx);
	void MacroConditionSelectionChanged(int idx);
	void MacroConditionReorder(int to, int target);
	void AddMacroCondition(int idx);
	void AddMacroCondition(Macro *macro, int idx, const std::string &id,
			       obs_data_t *data, Logic::Type logic);
	void RemoveMacroCondition(int idx);
	void MoveMacroConditionUp(int idx);
	void MoveMacroConditionDown(int idx);
	void HighlightControls() const;

signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString &newName);
	void MacroSegmentOrderChanged();
	void SegmentTempVarsChanged(MacroSegment *);

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

private:
	enum class MacroSection { CONDITIONS, ACTIONS, ELSE_ACTIONS };

	void PopulateMacroActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroElseActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroConditions(Macro &m, uint32_t afterIdx = 0);
	void SetupMacroSegmentSelection(MacroSection type, int idx);
	void
	SetupContextMenu(const QPoint &pos,
			 const std::function<void(MacroEdit *, int)> &remove,
			 const std::function<void(MacroEdit *)> &expand,
			 const std::function<void(MacroEdit *)> &collapse,
			 const std::function<void(MacroEdit *)> &maximize,
			 const std::function<void(MacroEdit *)> &minimize,
			 MacroSegmentList *list);
	void RunSegmentHighlightChecks();
	bool ElseSectionIsVisible() const;

	MacroSection lastInteracted = MacroSection::CONDITIONS;
	int currentConditionIdx = -1;
	int currentActionIdx = -1;
	int currentElseActionIdx = -1;

	std::shared_ptr<Macro> _currentMacro;

	std::unique_ptr<Ui_MacroEdit> ui;
};

} // namespace advss
