#pragma once
#ifdef BUILD_OUT_OF_TREE
#include "../../forms/ui_advanced-scene-switcher.h"
#elif defined ADVSS_MODULE
#include "../../../ui_advanced-scene-switcher.h"
#else
#include "ui_advanced-scene-switcher.h"
#endif
#include "switcher-data-structs.hpp"
#include "platform-funcs.hpp"

#define blog(level, msg, ...) blog(level, "[adv-ss] " msg, ##__VA_ARGS__)
#define vblog(level, msg, ...)                   \
	if (switcher->verbose) {                 \
		blog(level, msg, ##__VA_ARGS__); \
	}

class QCloseEvent;
class MacroActionEdit;
class MacroConditionEdit;

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

	void reject() override;
	void closeEvent(QCloseEvent *event) override;

	void SetShowFrames();
	void SetHideFrames();
	void clearFrames(QListWidget *list);

	int IgnoreWindowsFindByData(const QString &window);
	int IgnoreIdleWindowsFindByData(const QString &window);

	void UpdateNonMatchingScene(const QString &name);
	void OpenSequenceExtendEdit(SequenceWidget *sw);
	void SetEditSceneGroup(SceneGroup &sg);

	bool addNewMacro(std::string &name);
	Macro *getSelectedMacro();
	void SetEditMacro(Macro &m);
	void HighlightAction(int idx);
	void HighlightCondition(int idx);
	void PopulateMacroActions(Macro &m, uint32_t afterIdx = 0);
	void PopulateMacroConditions(Macro &m, uint32_t afterIdx = 0);
	void ConnectControlSignals(MacroActionEdit *);
	void ConnectControlSignals(MacroConditionEdit *);
	void SwapActions(Macro *m, int pos1, int pos2);
	void SwapConditions(Macro *m, int pos1, int pos2);

	void loadUI();
	void setupGeneralTab();
	void setupTitleTab();
	void setupExecutableTab();
	void setupRegionTab();
	void setupPauseTab();
	void setupSequenceTab();
	void setupTransitionsTab();
	void setupIdleTab();
	void setupRandomTab();
	void setupMediaTab();
	void setupFileTab();
	void setupTimeTab();
	void setupAudioTab();
	void setupSceneGroupTab();
	void setupTriggerTab();
	void setupVideoTab();
	void setupNetworkTab();
	void setupMacroTab();
	void setTabOrder();
	void restoreWindowGeo();
	void checkFirstTimeSetup();

signals:
	void MacroAdded(const QString &name);
	void MacroRemoved(const QString &name);
	void MacroRenamed(const QString &oldName, const QString newName);
	void SceneGroupAdded(const QString &name);
	void SceneGroupRemoved(const QString &name);
	void SceneGroupRenamed(const QString &oldName, const QString newName);

public slots:
	void on_windowUp_clicked();
	void on_windowDown_clicked();
	void on_windowAdd_clicked();
	void on_windowRemove_clicked();
	void on_noMatchDontSwitch_clicked();
	void on_noMatchSwitch_clicked();
	void on_noMatchRandomSwitch_clicked();
	void NoMatchDelayDurationChanged(double);
	void NoMatchDelayUnitChanged(DurationUnit);
	void CooldownDurationChanged(double);
	void CooldownUnitChanged(DurationUnit);
	void on_startupBehavior_currentIndexChanged(int index);
	void on_autoStartEvent_currentIndexChanged(int index);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_tabMoved(int from, int to);
	void on_tabWidget_currentChanged(int index);

	void on_macroAdd_clicked();
	void on_macroRemove_clicked();
	void on_macroUp_clicked();
	void on_macroDown_clicked();
	void on_macroName_editingFinished();
	void on_runMacro_clicked();
	void on_runMacroInParallel_stateChanged(int value);
	void on_runMacroOnChange_stateChanged(int value);
	void on_macros_currentRowChanged(int idx);
	void on_conditionAdd_clicked();
	void on_conditionRemove_clicked();
	void on_actionAdd_clicked();
	void on_actionRemove_clicked();
	void ShowMacroContextMenu(const QPoint &);
	void ShowMacroActionsContextMenu(const QPoint &);
	void ShowMacroConditionsContextMenu(const QPoint &);
	void CopyMacro();
	void ExpandAllActions();
	void ExpandAllConditions();
	void CollapseAllActions();
	void CollapseAllConditions();
	void MinimizeActions();
	void MinimizeConditions();
	void AddMacroAction(int idx);
	void RemoveMacroAction(int idx);
	void MoveMacroActionUp(int idx);
	void MoveMacroActionDown(int idx);
	void AddMacroCondition(int idx);
	void RemoveMacroCondition(int idx);
	void MoveMacroConditionUp(int idx);
	void MoveMacroConditionDown(int idx);
	void HighlightMatchedMacros();

	void on_screenRegionSwitches_currentRowChanged(int idx);
	void on_showFrame_clicked();
	void on_screenRegionAdd_clicked();
	void on_screenRegionRemove_clicked();
	void on_screenRegionUp_clicked();
	void on_screenRegionDown_clicked();

	void on_pauseUp_clicked();
	void on_pauseDown_clicked();
	void on_pauseAdd_clicked();
	void on_pauseRemove_clicked();

	void on_ignoreWindows_currentRowChanged(int idx);
	void on_ignoreWindowsAdd_clicked();
	void on_ignoreWindowsRemove_clicked();

	void on_sceneSequenceAdd_clicked();
	void on_sceneSequenceRemove_clicked();
	void on_sceneSequenceUp_clicked();
	void on_sceneSequenceDown_clicked();
	void on_sceneSequenceSave_clicked();
	void on_sceneSequenceLoad_clicked();
	void on_sequenceEdit_clicked();
	void on_sceneSequenceSwitches_itemDoubleClicked(QListWidgetItem *item);

	void on_verboseLogging_stateChanged(int state);
	void on_saveWindowGeo_stateChanged(int state);
	void on_showTrayNotifications_stateChanged(int state);
	void on_uiHintsDisable_stateChanged(int state);
	void on_useVerticalMacroControls_stateChanged(int state);
	void on_highlightExecutedMacros_stateChanged(int state);
	void on_hideLegacyTabs_stateChanged(int state);

	void on_exportSettings_clicked();
	void on_importSettings_clicked();

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
	void defTransitionDelayValueChanged(int value);

	void on_browseButton_clicked();
	void on_readFileCheckBox_stateChanged(int state);
	void on_readPathLineEdit_textChanged(const QString &text);
	void on_writePathLineEdit_textChanged(const QString &text);
	void on_browseButton_2_clicked();

	void on_executableUp_clicked();
	void on_executableDown_clicked();
	void on_executableAdd_clicked();
	void on_executableRemove_clicked();

	void on_idleCheckBox_stateChanged(int state);
	void on_ignoreIdleWindows_currentRowChanged(int idx);
	void on_ignoreIdleAdd_clicked();
	void on_ignoreIdleRemove_clicked();

	void on_randomAdd_clicked();
	void on_randomRemove_clicked();

	void on_fileAdd_clicked();
	void on_fileRemove_clicked();
	void on_fileSwitches_currentRowChanged(int idx);
	void on_fileUp_clicked();
	void on_fileDown_clicked();

	void on_mediaAdd_clicked();
	void on_mediaRemove_clicked();
	void on_mediaUp_clicked();
	void on_mediaDown_clicked();

	void on_timeAdd_clicked();
	void on_timeRemove_clicked();
	void on_timeUp_clicked();
	void on_timeDown_clicked();

	void on_audioAdd_clicked();
	void on_audioRemove_clicked();
	void on_audioUp_clicked();
	void on_audioDown_clicked();
	void on_audioFallback_toggled(bool on);

	void on_videoAdd_clicked();
	void on_videoRemove_clicked();
	void on_videoUp_clicked();
	void on_videoDown_clicked();
	void on_getScreenshot_clicked();

	void on_serverSettings_toggled(bool on);
	void on_serverPort_valueChanged(int value);
	void on_lockToIPv4_stateChanged(int state);
	void on_serverRestart_clicked();
	void updateServerStatus();
	void on_clientSettings_toggled(bool on);
	void on_clientHostname_textChanged(const QString &text);
	void on_clientPort_valueChanged(int value);
	void on_sendSceneChange_stateChanged(int state);
	void on_restrictSend_stateChanged(int state);
	void on_sendPreview_stateChanged(int state);
	void on_clientReconnect_clicked();
	void updateClientStatus();

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

	void on_triggerAdd_clicked();
	void on_triggerRemove_clicked();
	void on_triggerUp_clicked();
	void on_triggerDown_clicked();

	void on_priorityUp_clicked();
	void on_priorityDown_clicked();
	void on_threadPriority_currentTextChanged(const QString &text);

	void updateScreenRegionCursorPos();

	void on_close_clicked();

private:
};

/******************************************************************************
 * Sceneswitch helper
 ******************************************************************************/

void setNextTransition(const sceneSwitchInfo &ssi, obs_source_t *currentSource,
		       transitionData &td);
void overwriteTransitionOverride(const sceneSwitchInfo &ssi,
				 transitionData &td);
void restoreTransitionOverride(obs_source_t *scene, const transitionData &td);
void switchScene(const sceneSwitchInfo &ssi);
void switchPreviewScene(const OBSWeakSource &ws);

/******************************************************************************
 * Settings helper
 ******************************************************************************/
void AskForBackup(obs_data_t *obj);

/******************************************************************************
 * Main SwitcherData
 ******************************************************************************/
struct SwitcherData;
extern SwitcherData *switcher;
SwitcherData *GetSwitcher();
