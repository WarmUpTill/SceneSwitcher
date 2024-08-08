#pragma once
#include "macro-segment-list.hpp"
#include "condition-logic.hpp"
#include "log-helper.hpp"

#include <ui_advanced-scene-switcher.h>

class QCloseEvent;

namespace advss {

class MacroActionEdit;
class MacroConditionEdit;
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
	void on_warnPluginLoadFailure_stateChanged(int state);
	void on_hideLegacyTabs_stateChanged(int state);
	void on_priorityUp_clicked();
	void on_priorityDown_clicked();
	void on_threadPriority_currentTextChanged(const QString &text);

	/* --- End of legacy tab section --- */

	/* --- Begin of macro tab section --- */
public:
	void SetupMacroTab();
	bool AddNewMacro(std::shared_ptr<Macro> &res, std::string &name,
			 std::string format = "");
	void RemoveMacro(std::shared_ptr<Macro> &);
	void RemoveSelectedMacros();
	void RenameMacro(std::shared_ptr<Macro> &, const QString &name);
	std::shared_ptr<Macro> GetSelectedMacro();
	std::vector<std::shared_ptr<Macro>> GetSelectedMacros();
	void SetEditMacro(Macro &m);
	void SetMacroEditAreaDisabled(bool);
	void HighlightAction(int idx, QColor color = QColor(Qt::green));
	void HighlightElseAction(int idx, QColor color = QColor(Qt::green));
	void HighlightCondition(int idx, QColor color = QColor(Qt::green));
	void PopulateMacroActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroElseActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroConditions(Macro &m, uint32_t afterIdx = 0);
	void SetActionData(Macro &m);
	void SetElseActionData(Macro &m);
	void SetConditionData(Macro &m);
	void SwapActions(Macro *m, int pos1, int pos2);
	void SwapConditions(Macro *m, int pos1, int pos2);
	void HighligthMacroSettingsButton(bool enable = true);

public slots:
	void on_macroAdd_clicked();
	void on_macroRemove_clicked();
	void on_macroUp_clicked();
	void on_macroDown_clicked();
	void on_macroName_editingFinished();
	void on_runMacroInParallel_stateChanged(int value);
	void on_runMacroOnChange_stateChanged(int value);
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
	void on_toggleElseActions_clicked();
	void on_elseActionAdd_clicked();
	void on_elseActionRemove_clicked();
	void on_elseActionTop_clicked();
	void on_elseActionUp_clicked();
	void on_elseActionDown_clicked();
	void on_elseActionBottom_clicked();
	void MacroSelectionAboutToChange();
	void MacroSelectionChanged();
	void UpMacroSegementHotkey();
	void DownMacroSegementHotkey();
	void DeleteMacroSegementHotkey();
	void ShowMacroContextMenu(const QPoint &);
	void ShowMacroActionsContextMenu(const QPoint &);
	void ShowMacroElseActionsContextMenu(const QPoint &);
	void ShowMacroConditionsContextMenu(const QPoint &);
	void CopyMacro();
	void RenameSelectedMacro();
	void ExportMacros();
	void ImportMacros();
	void ExpandAllActions();
	void ExpandAllElseActions();
	void ExpandAllConditions();
	void CollapseAllActions();
	void CollapseAllElseActions();
	void CollapseAllConditions();
	void MinimizeActions();
	void MaximizeActions();
	void MinimizeElseActions();
	void MaximizeElseActions();
	void MinimizeConditions();
	void MaximizeConditions();
	void SetElseActionsStateToHidden();
	void SetElseActionsStateToVisible();
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
	void SwapElseActions(Macro *m, int pos1, int pos2);
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
	void HighlightControls();
	void HighlightOnChange();
	void on_macroSettings_clicked();
	void CopyMacroSegment();
	void PasteMacroSegment();

signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString &newName);
	void MacroSegmentOrderChanged();
	void SegmentTempVarsChanged();
	void HighlightMacrosChanged(bool value);

	void ConnectionAdded(const QString &);
	void ConnectionRenamed(const QString &oldName, const QString &newName);
	void ConnectionRemoved(const QString &);

private:
	enum class MacroSection { CONDITIONS, ACTIONS, ELSE_ACTIONS };

	void SetupMacroSegmentSelection(MacroSection type, int idx);
	bool ResolveMacroImportNameConflict(std::shared_ptr<Macro> &);
	bool MacroTabIsInFocus();

	MacroSection lastInteracted = MacroSection::CONDITIONS;
	int currentConditionIdx = -1;
	int currentActionIdx = -1;
	int currentElseActionIdx = -1;

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
	void SetupNetworkTab();
public slots:
	void on_serverSettings_toggled(bool on);
	void on_serverPort_valueChanged(int value);
	void on_lockToIPv4_stateChanged(int state);
	void on_serverRestart_clicked();
	void UpdateServerStatus();
	void on_clientSettings_toggled(bool on);
	void on_clientHostname_textChanged(const QString &text);
	void on_clientPort_valueChanged(int value);
	void on_sendSceneChange_stateChanged(int state);
	void on_restrictSend_stateChanged(int state);
	void on_sendPreview_stateChanged(int state);
	void on_clientReconnect_clicked();
	void UpdateClientStatus();

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

	// Trigger tab
public:
	void SetupTriggerTab();
public slots:
	void on_triggerAdd_clicked();
	void on_triggerRemove_clicked();
	void on_triggerUp_clicked();
	void on_triggerDown_clicked();

	/* --- End of legacy tab section --- */
};

void OpenSettingsWindow();
QWidget *GetSettingsWindow();
void HighligthMacroSettingsButton(bool enable);

} // namespace advss
