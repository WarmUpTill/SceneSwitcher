#pragma once

#include <QDialog>
#include <memory>
#include <vector>
#include <string>
#include "ui_advanced-scene-switcher.h"

struct obs_weak_source;
typedef struct obs_weak_source obs_weak_source_t;

class QCloseEvent;

class SceneSwitcher : public QDialog {
	Q_OBJECT

public:
	std::unique_ptr<Ui_SceneSwitcher> ui;
	bool loading = true;

	SceneSwitcher(QWidget *parent);

	void closeEvent(QCloseEvent *event) override;

	void SetStarted();
	void SetStopped();

	int FindByData(const QString &window);
	int ScreenRegionFindByData(const QString &region);
	int PauseScenesFindByData(const QString &scene);
	int PauseWindowsFindByData(const QString &window);
	int IgnoreWindowsFindByData(const QString &window);
	int SceneRoundTripFindByData(const QString &scene1);
	int SceneTransitionsFindByData(const QString &scene1, const QString &scene2);
	int executableFindByData(const QString &exe);//dasoven
	int IgnoreIdleWindowsFindByData(const QString& window);

	void UpdateNonMatchingScene(const QString &name);
	void UpdateAutoStopScene(const QString &name);
	void UpdateIdleDataTransition(const QString& name);
	void UpdateIdleDataScene(const QString& name);

public slots:
	void on_switches_currentRowChanged(int idx);
	void on_add_clicked();
	void on_remove_clicked();
	void on_noMatchDontSwitch_clicked();
	void on_noMatchSwitch_clicked();
	void on_startAtLaunch_toggled(bool value);
	void on_noMatchSwitchScene_currentTextChanged(const QString &text);
	void on_checkInterval_valueChanged(int value);
	void on_toggleStartButton_clicked();

	void on_screenRegions_currentRowChanged(int idx);
	void on_screenRegionAdd_clicked();
	void on_screenRegionRemove_clicked();

	void on_pauseScenes_currentRowChanged(int idx);
	void on_pauseScenesAdd_clicked();
	void on_pauseScenesRemove_clicked();

	void on_pauseWindows_currentRowChanged(int idx);
	void on_pauseWindowsAdd_clicked();
	void on_pauseWindowsRemove_clicked();

	void on_ignoreWindows_currentRowChanged(int idx);
	void on_ignoreWindowsAdd_clicked();
	void on_ignoreWindowsRemove_clicked();

	void on_sceneRoundTrips_currentRowChanged(int idx);
	void on_sceneRoundTripAdd_clicked();
	void on_sceneRoundTripRemove_clicked();
	void on_sceneRoundTripSave_clicked();
	void on_sceneRoundTripLoad_clicked();

	void on_autoStopSceneCheckBox_stateChanged(int state);
	void on_autoStopScenes_currentTextChanged(const QString &text);

	void on_sceneTransitions_currentRowChanged(int idx);
	void on_transitionsAdd_clicked();
	void on_transitionsRemove_clicked();

	void on_browseButton_clicked();
	void on_readFileCheckBox_stateChanged(int state);
	void on_readPathLineEdit_textChanged(const QString & text);
	void on_writePathLineEdit_textChanged(const QString & text);
	void on_browseButton_2_clicked();

	//dasoven region_start
	void on_executableAdd_clicked();
	void on_executableRemove_clicked();
	void on_executables_currentRowChanged(int idx);
	//dasoven region_end

	void on_idleCheckBox_stateChanged(int state);
	void on_idleTransitions_currentTextChanged(const QString& text);
	void on_idleScenes_currentTextChanged(const QString& text);
	void on_idleSpinBox_valueChanged(int i);
	void on_ignoreIdleWindows_currentRowChanged(int idx);
	void on_ignoreIdleAdd_clicked();
	void on_ignoreIdleRemove_clicked();

	void updateScreenRegionCursorPos();

	void on_close_clicked();
};

void GetWindowList(std::vector<std::string> &windows);
void GetCurrentWindowTitle(std::string &title);
std::pair<int, int> getCursorPos();
bool isFullscreen();
int getLastInputTime();
int getTime();

//dasoven region_start
bool isInFocus(const QString &exeToCheck);
void GetProcessList(QStringList &processes);
//dasoven region_end