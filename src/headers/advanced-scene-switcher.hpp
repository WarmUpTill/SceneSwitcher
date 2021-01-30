#pragma once
#ifdef BUILD_OUT_OF_TREE
#include "../../forms/ui_advanced-scene-switcher.h"
#else
#include "ui_advanced-scene-switcher.h"
#endif
#include "switcher-data-structs.hpp"

#define blog(level, msg, ...) blog(level, "[adv-ss] " msg, ##__VA_ARGS__)

class QCloseEvent;

/*******************************************************************************
 * Advanced Scene Switcher window
 *******************************************************************************/
class AdvSceneSwitcher : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_AdvSceneSwitcher> ui;
	bool loading = true;

	AdvSceneSwitcher(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

	void SetStarted();
	void SetStopped();

	void SetShowFrames();
	void SetHideFrames();
	void clearFrames(QListWidget *list);

	int IgnoreWindowsFindByData(const QString &window);
	int IgnoreIdleWindowsFindByData(const QString &window);

	void UpdateNonMatchingScene(const QString &name);
	void UpdateAutoStopScene(const QString &name);
	void UpdateAutoStartScene(const QString &name);
	void SetEditSceneGroup(SceneGroup &sg);

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
	void setTabOrder();

	static bool DisplayMessage(QString msg, bool question = false);
	static void AskBackup(obs_data_t *obj);

	static void populateSceneSelection(QComboBox *sel,
					   bool addPrevious = false,
					   bool addSceneGroup = false,
					   bool addSelect = true);

	static void populateTransitionSelection(QComboBox *sel,
						bool addSelect = true);
	static void populateWindowSelection(QComboBox *sel,
					    bool addSelect = true);
	static void populateAudioSelection(QComboBox *sel,
					   bool addSelect = true);
	static void populateMediaSelection(QComboBox *sel,
					   bool addSelect = true);
	static void populateProcessSelection(QComboBox *sel,
					     bool addSelect = true);
	QMetaObject::Connection PulseWidget(QWidget *widget, QColor endColor,
					    QColor = QColor(0, 0, 0, 0),
					    QString specifier = "QLabel ");

	void listAddClicked(QListWidget *list, SwitchWidget *newWidget,
			    QPushButton *addButton = nullptr,
			    QMetaObject::Connection *addHighlight = nullptr);
	bool listMoveUp(QListWidget *list);
	bool listMoveDown(QListWidget *list);

signals:
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
	void on_noMatchDelay_valueChanged(double i);
	void on_cooldownTime_valueChanged(double i);
	void on_startupBehavior_currentIndexChanged(int index);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_toggleStartButton_clicked();
	void on_tabMoved(int from, int to);
	void on_tabWidget_currentChanged(int index);

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
	void on_sceneSequenceSave_clicked();
	void on_sceneSequenceLoad_clicked();
	void on_sceneSequenceUp_clicked();
	void on_sceneSequenceDown_clicked();

	void on_autoStopSceneCheckBox_stateChanged(int state);
	void on_autoStopType_currentIndexChanged(int index);
	void on_autoStopScenes_currentTextChanged(const QString &text);

	void on_autoStartSceneCheckBox_stateChanged(int state);
	void on_autoStartType_currentIndexChanged(int index);
	void on_autoStartScenes_currentTextChanged(const QString &text);

	void on_verboseLogging_stateChanged(int state);
	void on_uiHintsDisable_stateChanged(int state);

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

/********************************************************************************
 * Windowtitle helper
 ********************************************************************************/
void GetWindowList(std::vector<std::string> &windows);
void GetWindowList(QStringList &windows); // Overloaded
void GetCurrentWindowTitle(std::string &title);
bool isFullscreen(std::string &title);
bool isMaximized(std::string &title);

/********************************************************************************
 * Screenregion helper
 ********************************************************************************/
std::pair<int, int> getCursorPos();

/********************************************************************************
 * Idle detection helper
 ********************************************************************************/
int secondsSinceLastInput();

/********************************************************************************
 * Executable helper
 ********************************************************************************/
void GetProcessList(QStringList &processes);
bool isInFocus(const QString &executable);

/********************************************************************************
 * Sceneswitch helper
 ********************************************************************************/

void setNextTransition(OBSWeakSource &targetScene, obs_source_t *currentSource,
		       OBSWeakSource &transition,
		       bool &transitionOverrideOverride, transitionData &td);
void overwriteTransitionOverride(obs_weak_source_t *sceneWs,
				 obs_source_t *transition, transitionData &td);
void restoreTransitionOverride(obs_source_t *scene, transitionData td);
void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 bool &transitionOverrideOverride);



/********************************************************************************
 * Main SwitcherData
 ********************************************************************************/
struct SwitcherData;
extern SwitcherData *switcher;
