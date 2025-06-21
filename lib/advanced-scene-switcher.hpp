#pragma once
#include "macro-segment-list.hpp"
#include "condition-logic.hpp"
#include "log-helper.hpp"

#include <ui_advanced-scene-switcher.h>

class QCloseEvent;

namespace advss {

class MacroActionEdit;
class MacroConditionEdit;
class MacroSegment;
class Duration;
class SequenceWidget;
struct SceneGroup;

/*******************************************************************************
 * Advanced Scene Switcher window
 *******************************************************************************/
class AdvSceneSwitcher : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_AdvSceneSwitcher> ui;
	bool loading = true;

	AdvSceneSwitcher(QWidget *parent);
	~AdvSceneSwitcher();

	static AdvSceneSwitcher *window;

	void reject() override;
	void closeEvent(QCloseEvent *event) override;

	void LoadUI();

	void RestoreWindowGeo();
	void CheckFirstTimeSetup();

protected:
	bool eventFilter(QObject *obj, QEvent *event) override;

	/* --- Begin of general tab section --- */
public:
	void SetupGeneralTab();
	void UpdateNonMatchingScene(const QString &name);
	void SetDeprecationWarnings();

public slots:
	void on_noMatchDontSwitch_clicked();
	void on_noMatchSwitch_clicked();
	void on_noMatchRandomSwitch_clicked();
	void NoMatchDelayDurationChanged(const Duration &);
	void CooldownDurationChanged(const Duration &);
	void on_enableCooldown_stateChanged(int state);
	void on_startupBehavior_currentIndexChanged(int index);
	void on_logLevel_currentIndexChanged(int index);
	void on_autoStartEvent_currentIndexChanged(int index);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_tabWidget_currentChanged(int index);
	void on_exportSettings_clicked();
	void on_importSettings_clicked();
	void on_saveWindowGeo_stateChanged(int state);
	void on_showTrayNotifications_stateChanged(int state);
	void on_uiHintsDisable_stateChanged(int state);
	void on_disableComboBoxFilter_stateChanged(int state);
	void on_disableMacroWidgetCache_stateChanged(int state);
	void on_warnPluginLoadFailure_stateChanged(int state);
	void on_hideLegacyTabs_stateChanged(int state);
	void on_priorityUp_clicked();
	void on_priorityDown_clicked();
	void on_threadPriority_currentTextChanged(const QString &text);

	/* --- End of legacy tab section --- */

	/* --- Begin of macro tab section --- */
public:
	void SetupMacroTab();
	bool MacroTabIsInFocus();
	bool AddNewMacro(std::shared_ptr<Macro> &res, std::string &name,
			 std::string format = "");
	void RemoveMacro(std::shared_ptr<Macro> &);
	void RemoveSelectedMacros();
	void RenameMacro(std::shared_ptr<Macro> &, const QString &name);
	std::shared_ptr<Macro> GetSelectedMacro() const;
	std::vector<std::shared_ptr<Macro>> GetSelectedMacros() const;
	void SetMacroEditAreaDisabled(bool) const;
	void HighlightAction(int idx, QColor color = QColor(Qt::green)) const;
	void HighlightElseAction(int idx,
				 QColor color = QColor(Qt::green)) const;
	void HighlightCondition(int idx,
				QColor color = QColor(Qt::green)) const;
	void PopulateMacroActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroElseActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroConditions(Macro &m, uint32_t afterIdx = 0);
	void SetActionData(Macro &m) const;
	void SetElseActionData(Macro &m) const;
	void SetConditionData(Macro &m) const;
	void SwapActions(Macro *m, int pos1, int pos2);
	void SwapConditions(Macro *m, int pos1, int pos2);
	void HighlightMacroSettingsButton(bool enable = true);

public slots:
	void on_macroAdd_clicked();
	void on_macroRemove_clicked();
	void on_macroUp_clicked() const;
	void on_macroDown_clicked() const;
	void on_macroName_editingFinished();
	void on_runMacroInParallel_stateChanged(int value) const;
	void on_runMacroOnChange_stateChanged(int value) const;
	void MacroSelectionChanged();
	void ShowMacroContextMenu(const QPoint &);
	void CopyMacro();
	void RenameSelectedMacro();
	void ExportMacros() const;
	void ImportMacros();
	void HighlightOnChange() const;
	void on_macroSettings_clicked();

signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString &newName);
	void MacroSegmentOrderChanged();
	void SegmentTempVarsChanged(MacroSegment *);
	void HighlightMacrosChanged(bool value);

	void ConnectionAdded(const QString &);
	void ConnectionRenamed(const QString &oldName, const QString &newName);
	void ConnectionRemoved(const QString &);

private:
	bool ResolveMacroImportNameConflict(std::shared_ptr<Macro> &);

	/* --- End of macro tab section --- */

	/* --- Begin of legacy tab section --- */
public:
	void ClearFrames(QListWidget *list);
	int IgnoreWindowsFindByData(const QString &window);
	int IgnoreIdleWindowsFindByData(const QString &window);
	void OpenSequenceExtendEdit(SequenceWidget *sw);

	// Window tab
public:
	void SetupTitleTab();
public slots:
	void on_windowUp_clicked();
	void on_windowDown_clicked();
	void on_windowAdd_clicked();
	void on_windowRemove_clicked();
	void on_ignoreWindows_currentRowChanged(int idx);
	void on_ignoreWindowsAdd_clicked();
	void on_ignoreWindowsRemove_clicked();

	// Screen region tab
public:
	void SetupRegionTab();
public slots:
	void SetShowFrames();
	void SetHideFrames();
	void on_screenRegionSwitches_currentRowChanged(int idx);
	void on_showFrame_clicked();
	void on_screenRegionAdd_clicked();
	void on_screenRegionRemove_clicked();
	void on_screenRegionUp_clicked();
	void on_screenRegionDown_clicked();
	void updateScreenRegionCursorPos();

	// Pause tab
public:
	void SetupPauseTab();
public slots:
	void on_pauseUp_clicked();
	void on_pauseDown_clicked();
	void on_pauseAdd_clicked();
	void on_pauseRemove_clicked();

	// Sequence tab
public:
	void SetupSequenceTab();
public slots:
	void on_sceneSequenceAdd_clicked();
	void on_sceneSequenceRemove_clicked();
	void on_sceneSequenceUp_clicked();
	void on_sceneSequenceDown_clicked();
	void on_sceneSequenceSave_clicked();
	void on_sceneSequenceLoad_clicked();
	void on_sequenceEdit_clicked();
	void on_sceneSequenceSwitches_itemDoubleClicked(QListWidgetItem *item);

	// Transition tab
public:
	void SetupTransitionsTab();
public slots:
	void on_transitionsAdd_clicked();
	void on_transitionsRemove_clicked();
	void on_transitionsUp_clicked();
	void on_transitionsDown_clicked();
	void on_defaultTransitionsAdd_clicked();
	void on_defaultTransitionsRemove_clicked();
	void on_defaultTransitionsUp_clicked();
	void on_defaultTransitionsDown_clicked();
	void on_transitionOverridecheckBox_stateChanged(int state);
	void on_adjustActiveTransitionType_stateChanged(int state);
	void DefTransitionDelayValueChanged(int value);

	// Executable tab
public:
	void SetupExecutableTab();
public slots:
	void on_executableUp_clicked();
	void on_executableDown_clicked();
	void on_executableAdd_clicked();
	void on_executableRemove_clicked();

	// Idle tab
public:
	void SetupIdleTab();
public slots:
	void on_idleCheckBox_stateChanged(int state);
	void on_ignoreIdleWindows_currentRowChanged(int idx);
	void on_ignoreIdleAdd_clicked();
	void on_ignoreIdleRemove_clicked();

	// Random tab
public:
	void SetupRandomTab();
public slots:
	void on_randomAdd_clicked();
	void on_randomRemove_clicked();

	// File tab
public:
	void SetupFileTab();
public slots:
	void on_fileAdd_clicked();
	void on_fileRemove_clicked();
	void on_fileSwitches_currentRowChanged(int idx);
	void on_fileUp_clicked();
	void on_fileDown_clicked();
	void on_browseButton_clicked();
	void on_readFileCheckBox_stateChanged(int state);
	void on_readPathLineEdit_textChanged(const QString &text);
	void on_writePathLineEdit_textChanged(const QString &text);
	void on_browseButton_2_clicked();

	// Media tab
public:
	void SetupMediaTab();
public slots:
	void on_mediaAdd_clicked();
	void on_mediaRemove_clicked();
	void on_mediaUp_clicked();
	void on_mediaDown_clicked();

	// Time tab
public:
	void SetupTimeTab();
public slots:
	void on_timeAdd_clicked();
	void on_timeRemove_clicked();
	void on_timeUp_clicked();
	void on_timeDown_clicked();

	// Audio tab
public:
	void SetupAudioTab();
public slots:
	void on_audioAdd_clicked();
	void on_audioRemove_clicked();
	void on_audioUp_clicked();
	void on_audioDown_clicked();
	void on_audioFallback_toggled(bool on);

	// Video tab
public:
	void SetupVideoTab();
public slots:
	void on_videoAdd_clicked();
	void on_videoRemove_clicked();
	void on_videoUp_clicked();
	void on_videoDown_clicked();
	void on_getScreenshot_clicked();

	// Scene group tab
public:
	void SetupSceneGroupTab();
	void SetEditSceneGroup(SceneGroup &sg);
public slots:
	void on_sceneGroupAdd_clicked();
	void on_sceneGroupRemove_clicked();
	void on_sceneGroupUp_clicked();
	void on_sceneGroupDown_clicked();
	void on_sceneGroupName_editingFinished();
	void on_sceneGroups_currentRowChanged(int idx);
	void on_sceneGroupSceneAdd_clicked();
	void on_sceneGroupSceneRemove_clicked();
	void on_sceneGroupSceneUp_clicked();
	void on_sceneGroupSceneDown_clicked();

signals:
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);

	/* --- End of legacy tab section --- */
private:
	void SetCheckIntervalTooLowVisibility() const;
};

void OpenSettingsWindow();
void HighlightMacroSettingsButton(bool enable);

} // namespace advss
