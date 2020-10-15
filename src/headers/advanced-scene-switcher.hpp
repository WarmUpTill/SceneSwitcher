#pragma once

#include <QDialog>
#include <memory>
#include <vector>
#include <string>
#ifdef BUILD_OUT_OF_TREE
#include "../../forms/ui_advanced-scene-switcher.h"
#else
#include "ui_advanced-scene-switcher.h"
#endif
#include "switcher-data-structs.hpp"
#include "volume-control.hpp"

#define blog(level, msg, ...) blog(level, "[adv-ss] " msg, ##__VA_ARGS__)

class QCloseEvent;

/*******************************************************************************
 * Advanced Scene Switcher window
 *******************************************************************************/
class SceneSwitcher : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_SceneSwitcher> ui;
	bool loading = true;

	SceneSwitcher(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

	void SetStarted();
	void SetStopped();

	int ScreenRegionFindByData(const QString &region);
	int PauseScenesFindByData(const QString &scene);
	int PauseWindowsFindByData(const QString &window);
	int IgnoreWindowsFindByData(const QString &window);
	int SceneSequenceFindByData(const QString &scene1);
	int SceneTransitionsFindByData(const QString &scene1,
				       const QString &scene2);
	int DefaultTransitionsFindByData(const QString &scene);
	int IgnoreIdleWindowsFindByData(const QString &window);

	void UpdateNonMatchingScene(const QString &name);
	void UpdateAutoStopScene(const QString &name);
	void UpdateAutoStartScene(const QString &name);
	void UpdateIdleDataTransition(const QString &name);
	void UpdateIdleDataScene(const QString &name);

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
	void setTabOrder();

	static void populateSceneSelection(QComboBox *sel,
					   bool addPrevious = false,
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
					    QString specifier = "");

	bool listMoveUp(QListWidget *list);
	bool listMoveDown(QListWidget *list);

public slots:
	void on_windowUp_clicked();
	void on_windowDown_clicked();
	void on_windowAdd_clicked();
	void on_windowRemove_clicked();
	void on_noMatchDontSwitch_clicked();
	void on_noMatchSwitch_clicked();
	void on_noMatchRandomSwitch_clicked();
	void on_startupBehavior_currentIndexChanged(int index);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_toggleStartButton_clicked();
	void on_tabMoved(int from, int to);

	void on_screenRegions_currentRowChanged(int idx);
	void on_screenRegionAdd_clicked();
	void on_screenRegionRemove_clicked();
	void on_screenRegionUp_clicked();
	void on_screenRegionDown_clicked();

	void on_pauseScenes_currentRowChanged(int idx);
	void on_pauseScenesAdd_clicked();
	void on_pauseScenesRemove_clicked();

	void on_pauseWindows_currentRowChanged(int idx);
	void on_pauseWindowsAdd_clicked();
	void on_pauseWindowsRemove_clicked();

	void on_ignoreWindows_currentRowChanged(int idx);
	void on_ignoreWindowsAdd_clicked();
	void on_ignoreWindowsRemove_clicked();

	void on_sceneSequences_currentRowChanged(int idx);
	void on_sceneSequenceAdd_clicked();
	void on_sceneSequenceRemove_clicked();
	void on_sceneSequenceSave_clicked();
	void on_sceneSequenceLoad_clicked();
	void on_sceneSequenceUp_clicked();
	void on_sceneSequenceDown_clicked();
	void on_sceneSequenceDelayUnits_currentIndexChanged(int index);

	void on_autoStopSceneCheckBox_stateChanged(int state);
	void on_autoStopScenes_currentTextChanged(const QString &text);

	void on_autoStartSceneCheckBox_stateChanged(int state);
	void on_autoStartType_currentIndexChanged(int index);
	void on_autoStartScenes_currentTextChanged(const QString &text);

	void on_verboseLogging_stateChanged(int state);
	void on_uiHintsDisable_stateChanged(int state);

	void on_exportSettings_clicked();
	void on_importSettings_clicked();

	void on_sceneTransitions_currentRowChanged(int idx);
	void on_transitionsAdd_clicked();
	void on_transitionsRemove_clicked();
	void on_defaultTransitions_currentRowChanged(int idx);
	void on_defaultTransitionsAdd_clicked();
	void on_defaultTransitionsRemove_clicked();
	void on_transitionOverridecheckBox_stateChanged(int state);

	void on_browseButton_clicked();
	void on_readFileCheckBox_stateChanged(int state);
	void on_fileType_currentIndexChanged(int idx);
	void on_readPathLineEdit_textChanged(const QString &text);
	void on_writePathLineEdit_textChanged(const QString &text);
	void on_browseButton_2_clicked();

	void on_executableUp_clicked();
	void on_executableDown_clicked();
	void on_executableAdd_clicked();
	void on_executableRemove_clicked();

	void on_idleCheckBox_stateChanged(int state);
	void on_idleTransitions_currentTextChanged(const QString &text);
	void on_idleScenes_currentTextChanged(const QString &text);
	void on_idleSpinBox_valueChanged(int i);
	void on_ignoreIdleWindows_currentRowChanged(int idx);
	void on_ignoreIdleAdd_clicked();
	void on_ignoreIdleRemove_clicked();

	void on_randomAdd_clicked();
	void on_randomRemove_clicked();

	void on_fileAdd_clicked();
	void on_fileRemove_clicked();
	void on_fileScenesList_currentRowChanged(int idx);
	void on_browseButton_3_clicked();
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
struct obs_weak_source;
typedef struct obs_weak_source obs_weak_source_t;

typedef struct transitionData {
	std::string name = "";
	int duration = 0;
} transitionData;

void setNextTransition(OBSWeakSource &targetScene, obs_source_t *currentSource,
		       OBSWeakSource &transition,
		       bool &transitionOverrideOverride, transitionData &td);
void overwriteTransitionOverride(obs_weak_source_t *sceneWs,
				 obs_source_t *transition, transitionData &td);
void restoreTransitionOverride(obs_source_t *scene, transitionData td);

void switchScene(OBSWeakSource &scene, OBSWeakSource &transition,
		 bool &transitionOverrideOverride,
		 std::unique_lock<std::mutex> &lock);

/********************************************************************************
 * Main SwitcherData
 ********************************************************************************/
struct SwitcherData;
extern SwitcherData *switcher;
