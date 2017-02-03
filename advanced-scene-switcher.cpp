#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>

#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include <QDir>
#include <QFileDialog>
#include <QTextStream>
#include <QTimer>

#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>
#include <fstream>

#include "switcher-data-structs.hpp"
#include "utility.hpp"
#include "advanced-scene-switcher.hpp"



using namespace std;

struct SwitcherData
{
	thread th;
	condition_variable cv;
	mutex m;
	mutex waitMutex;
	mutex transitionEndMutex;
	bool transitionActive = false;
	mutex threadEndMutex;
	condition_variable transitionCv;
	bool stop = false;

	vector<SceneSwitch> switches;
	OBSWeakSource nonMatchingScene;
	int interval = DEFAULT_INTERVAL;
	bool switchIfNotMatching = false;
	bool startAtLaunch = false;

	vector<ScreenRegionSwitch> screenRegionSwitches;

	vector<OBSWeakSource> pauseScenesSwitches;

	vector<string> pauseWindowsSwitches;

	vector<string> ignoreWindowsSwitches;

	vector<SceneRoundTripSwitch> sceneRoundTripSwitches;

	bool autoStopEnable = false;
	OBSWeakSource autoStopScene;

	vector<SceneTransition> sceneTransitions;
	vector<DefaultSceneTransition> defaultSceneTransitions;

	vector<ExecutableSceneSwitch> executableSwitches;

	FileIOData fileIO;

	IdleData idleData;
	vector<string> ignoreIdleWindows;

	void Thread();
	void Start();
	void Stop();

	void Prune()
	{
		for (size_t i = 0; i < switches.size(); i++)
		{
			SceneSwitch& s = switches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				switches.erase(switches.begin() + i--);
		}

		if (nonMatchingScene && !WeakSourceValid(nonMatchingScene))
		{
			switchIfNotMatching = false;
			nonMatchingScene = nullptr;
		}

		for (size_t i = 0; i < screenRegionSwitches.size(); i++)
		{
			ScreenRegionSwitch& s = screenRegionSwitches[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				screenRegionSwitches.erase(screenRegionSwitches.begin() + i--);
		}

		for (size_t i = 0; i < pauseScenesSwitches.size(); i++)
		{
			OBSWeakSource& scene = pauseScenesSwitches[i];
			if (!WeakSourceValid(scene))
				pauseScenesSwitches.erase(pauseScenesSwitches.begin() + i--);
		}

		for (size_t i = 0; i < sceneRoundTripSwitches.size(); i++)
		{
			SceneRoundTripSwitch& s = sceneRoundTripSwitches[i];
			if (!WeakSourceValid(s.scene1) || !WeakSourceValid(s.scene2)
				|| !WeakSourceValid(s.transition))
				sceneRoundTripSwitches.erase(sceneRoundTripSwitches.begin() + i--);
		}

		if (!WeakSourceValid(autoStopScene))
		{
			autoStopScene = NULL;
			autoStopEnable = false;
		}

		for (size_t i = 0; i < sceneTransitions.size(); i++)
		{
			SceneTransition& s = sceneTransitions[i];
			if (!WeakSourceValid(s.scene1) || !WeakSourceValid(s.scene2)
				|| !WeakSourceValid(s.transition))
				sceneTransitions.erase(sceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < defaultSceneTransitions.size(); i++)
		{
			DefaultSceneTransition& s = defaultSceneTransitions[i];
			if (!WeakSourceValid(s.scene) || !WeakSourceValid(s.transition))
				defaultSceneTransitions.erase(defaultSceneTransitions.begin() + i--);
		}

		for (size_t i = 0; i < executableSwitches.size(); i++)
		{
			ExecutableSceneSwitch& s = executableSwitches[i];
			if (!WeakSourceValid(s.mScene) || !WeakSourceValid(s.mTransition))
				switches.erase(switches.begin() + i--);
		}

		if (!WeakSourceValid(idleData.scene) || !WeakSourceValid(idleData.transition))
		{
			idleData.scene = NULL;
			idleData.transition = NULL;
			idleData.idleEnable = false;
		}
	}

	inline ~SwitcherData()
	{
		Stop();
	}
};

static SwitcherData* switcher = nullptr;

SceneSwitcher::SceneSwitcher(QWidget* parent)
	: QDialog(parent)
	, ui(new Ui_SceneSwitcher)
{
	ui->setupUi(this);

	lock_guard<mutex> lock(switcher->m);

	switcher->Prune();

	BPtr<char*> scenes = obs_frontend_get_scene_names();
	char** temp = scenes;
	while (*temp)
	{
		const char* name = *temp;
		ui->scenes->addItem(name);
		ui->noMatchSwitchScene->addItem(name);
		ui->screenRegionScenes->addItem(name);
		ui->pauseScenesScenes->addItem(name);
		ui->sceneRoundTripScenes1->addItem(name);
		ui->sceneRoundTripScenes2->addItem(name);
		ui->autoStopScenes->addItem(name);
		ui->transitionsScene1->addItem(name);
		ui->transitionsScene2->addItem(name);
		ui->defaultTransitionsScene->addItem(name);
		ui->executableScenes->addItem(name);
		ui->idleScenes->addItem(name);
		temp++;
	}

	obs_frontend_source_list* transitions = new obs_frontend_source_list();
	obs_frontend_get_transitions(transitions);

	for (size_t i = 0; i < transitions->sources.num; i++)
	{
		const char* name = obs_source_get_name(transitions->sources.array[i]);
		ui->transitions->addItem(name);
		ui->screenRegionsTransitions->addItem(name);
		ui->sceneRoundTripTransitions->addItem(name);
		ui->transitionsTransitions->addItem(name);
		ui->defaultTransitionsTransitions->addItem(name);
		ui->executableTransitions->addItem(name);
		ui->idleTransitions->addItem(name);
	}

	obs_frontend_source_list_free(transitions);

	if (switcher->switchIfNotMatching)
		ui->noMatchSwitch->setChecked(true);
	else
		ui->noMatchDontSwitch->setChecked(true);

	ui->noMatchSwitchScene->setCurrentText(GetWeakSourceName(switcher->nonMatchingScene).c_str());
	ui->checkInterval->setValue(switcher->interval);

	vector<string> windows;
	GetWindowList(windows);

	for (string& window : windows)
	{
		ui->windows->addItem(window.c_str());
		ui->ignoreWindowsWindows->addItem(window.c_str());
		ui->pauseWindowsWindows->addItem(window.c_str());
		ui->ignoreIdleWindowsWindows->addItem(window.c_str());
	}

	QStringList processes;
	GetProcessList(processes);
	for (QString& process : processes)
		ui->executable->addItem(process);

	for (auto& s : switcher->executableSwitches)
	{
		string sceneName = GetWeakSourceName(s.mScene);
		string transitionName = GetWeakSourceName(s.mTransition);
		QString text = MakeSwitchNameExecutable(
			sceneName.c_str(), s.mExe, transitionName.c_str(), s.mInFocus);

		QListWidgetItem* item = new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, s.mExe);
	}

	for (auto& s : switcher->switches)
	{
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSwitchName(
			sceneName.c_str(), s.window.c_str(), transitionName.c_str(), s.fullscreen);

		QListWidgetItem* item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, s.window.c_str());
	}

	for (auto& s : switcher->screenRegionSwitches)
	{
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeScreenRegionSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.minX, s.minY, s.maxX, s.maxY);

		QListWidgetItem* item = new QListWidgetItem(text, ui->screenRegions);
		item->setData(Qt::UserRole, s.regionStr.c_str());
	}

	ui->autoStopSceneCheckBox->setChecked(switcher->autoStopEnable);
	ui->autoStopScenes->setCurrentText(GetWeakSourceName(switcher->autoStopScene).c_str());

	if (ui->autoStopSceneCheckBox->checkState())
	{
		ui->autoStopScenes->setDisabled(false);
	}
	else
	{
		ui->autoStopScenes->setDisabled(true);
	}

	for (auto& scene : switcher->pauseScenesSwitches)
	{
		string sceneName = GetWeakSourceName(scene);
		QString text = QString::fromStdString(sceneName);

		QListWidgetItem* item = new QListWidgetItem(text, ui->pauseScenes);
		item->setData(Qt::UserRole, text);
	}

	for (auto& window : switcher->pauseWindowsSwitches)
	{
		QString text = QString::fromStdString(window);

		QListWidgetItem* item = new QListWidgetItem(text, ui->pauseWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto& window : switcher->ignoreWindowsSwitches)
	{
		QString text = QString::fromStdString(window);

		QListWidgetItem* item = new QListWidgetItem(text, ui->ignoreWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto& s : switcher->sceneRoundTripSwitches)
	{
		string sceneName1 = GetWeakSourceName(s.scene1);
		string sceneName2 = GetWeakSourceName(s.scene2);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneRoundTripSwitchName(
			sceneName1.c_str(), sceneName2.c_str(), transitionName.c_str(), s.delay);

		QListWidgetItem* item = new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, text);
	}

	for (auto& s : switcher->sceneTransitions)
	{
		string sceneName1 = GetWeakSourceName(s.scene1);
		string sceneName2 = GetWeakSourceName(s.scene2);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeSceneTransitionName(
			sceneName1.c_str(), sceneName2.c_str(), transitionName.c_str());

		QListWidgetItem* item = new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, text);
	}

	for (auto& s : switcher->defaultSceneTransitions)
	{
		string sceneName = GetWeakSourceName(s.scene);
		string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeDefaultSceneTransitionName(
			sceneName.c_str(), transitionName.c_str());

		QListWidgetItem* item = new QListWidgetItem(text, ui->defaultTransitions);
		item->setData(Qt::UserRole, text);
	}

	for (auto& window : switcher->ignoreIdleWindows)
	{
		QString text = QString::fromStdString(window);

		QListWidgetItem* item = new QListWidgetItem(text, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, text);
	}

	ui->idleCheckBox->setChecked(switcher->idleData.idleEnable);
	ui->idleScenes->setCurrentText(GetWeakSourceName(switcher->idleData.scene).c_str());
	ui->idleTransitions->setCurrentText(GetWeakSourceName(switcher->idleData.transition).c_str());
	ui->idleSpinBox->setValue(switcher->idleData.time);

	if (ui->idleCheckBox->checkState())
	{
		ui->idleScenes->setDisabled(false);
		ui->idleSpinBox->setDisabled(false);
		ui->idleTransitions->setDisabled(false);
	}
	else
	{
		ui->idleScenes->setDisabled(true);
		ui->idleSpinBox->setDisabled(true);
		ui->idleTransitions->setDisabled(true);
	}

	ui->readPathLineEdit->setText(QString::fromStdString(switcher->fileIO.readPath.c_str()));
	ui->readFileCheckBox->setChecked(switcher->fileIO.readEnabled);
	ui->writePathLineEdit->setText(QString::fromStdString(switcher->fileIO.writePath.c_str()));

	if (ui->readFileCheckBox->checkState())
	{
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
	}
	else
	{
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
	}

	if (switcher->th.joinable())
		SetStarted();
	else
		SetStopped();

	loading = false;

	// screen region cursor position
	QTimer* screenRegionTimer = new QTimer(this);
	connect(screenRegionTimer, SIGNAL(timeout()), this, SLOT(updateScreenRegionCursorPos()));
	screenRegionTimer->start(1000);
}

void SceneSwitcher::closeEvent(QCloseEvent*)
{
	obs_frontend_save();
}

int SceneSwitcher::FindByData(const QString& window)
{
	int count = ui->switches->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->switches->item(i);
		QString itemWindow = item->data(Qt::UserRole).toString();

		if (itemWindow == window)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::ScreenRegionFindByData(const QString& region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->screenRegions->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == region)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::PauseScenesFindByData(const QString& scene)
{
	int count = ui->pauseScenes->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->pauseScenes->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == scene)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::PauseWindowsFindByData(const QString& window)
{
	int count = ui->pauseWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->pauseWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::IgnoreWindowsFindByData(const QString& window)
{
	int count = ui->ignoreWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->ignoreWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::SceneRoundTripFindByData(const QString& scene1)
{
	QRegExp rx(scene1 + " -> wait for [0-9]* seconds -> .*");
	int count = ui->sceneRoundTrips->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->sceneRoundTrips->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString))
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::SceneTransitionsFindByData(const QString& scene1, const QString& scene2)
{
	QRegExp rx(scene1 + " --- .* --> " + scene2);
	int count = ui->sceneTransitions->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->sceneTransitions->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString))
		{
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::DefaultTransitionsFindByData(const QString& scene)
{
	QRegExp rx(scene + " --> .*");
	int count = ui->defaultTransitions->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->defaultTransitions->item(i);
		QString itemString = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString))
		{
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_switches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->switches->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->switches)
	{
		if (window.compare(s.window.c_str()) == 0)
		{
			string name = GetWeakSourceName(s.scene);
			string transitionName = GetWeakSourceName(s.transition);
			ui->scenes->setCurrentText(name.c_str());
			ui->windows->setCurrentText(window);
			ui->transitions->setCurrentText(transitionName.c_str());
			ui->fullscreenCheckBox->setChecked(s.fullscreen);
			break;
		}
	}
}

void SceneSwitcher::on_screenRegions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->screenRegions->item(idx);

	QString region = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->screenRegionSwitches)
	{
		if (region.compare(s.regionStr.c_str()) == 0)
		{
			string name = GetWeakSourceName(s.scene);
			string transitionName = GetWeakSourceName(s.transition);
			ui->screenRegionScenes->setCurrentText(name.c_str());
			ui->screenRegionsTransitions->setCurrentText(transitionName.c_str());
			ui->screenRegionMinX->setValue(s.minX);
			ui->screenRegionMinY->setValue(s.minY);
			ui->screenRegionMaxX->setValue(s.maxX);
			ui->screenRegionMaxY->setValue(s.maxY);
			break;
		}
	}
}

void SceneSwitcher::on_pauseScenes_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->pauseScenes->item(idx);

	QString scene = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->pauseScenesSwitches)
	{
		string name = GetWeakSourceName(s);
		if (scene.compare(name.c_str()) == 0)
		{
			ui->pauseScenesScenes->setCurrentText(name.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_pauseWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->pauseWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->pauseWindowsSwitches)
	{
		if (window.compare(s.c_str()) == 0)
		{
			ui->pauseWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_ignoreWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->ignoreWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->ignoreWindowsSwitches)
	{
		if (window.compare(s.c_str()) == 0)
		{
			ui->ignoreWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_sceneRoundTrips_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->sceneRoundTrips->item(idx);

	QString sceneRoundTrip = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->sceneRoundTripSwitches)
	{
		if (sceneRoundTrip.compare(s.sceneRoundTripStr.c_str()) == 0)
		{
			string scene1 = GetWeakSourceName(s.scene1);
			string scene2 = GetWeakSourceName(s.scene2);
			string transitionName = GetWeakSourceName(s.transition);
			int delay = s.delay;
			ui->sceneRoundTripScenes1->setCurrentText(scene1.c_str());
			ui->sceneRoundTripScenes2->setCurrentText(scene2.c_str());
			ui->sceneRoundTripTransitions->setCurrentText(transitionName.c_str());
			ui->sceneRoundTripSpinBox->setValue(delay);
			break;
		}
	}
}

void SceneSwitcher::on_sceneTransitions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->sceneTransitions->item(idx);

	QString sceneTransition = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->sceneTransitions)
	{
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) == 0)
		{
			string scene1 = GetWeakSourceName(s.scene1);
			string scene2 = GetWeakSourceName(s.scene2);
			string transitionName = GetWeakSourceName(s.transition);
			ui->transitionsScene1->setCurrentText(scene1.c_str());
			ui->transitionsScene2->setCurrentText(scene2.c_str());
			ui->transitionsTransitions->setCurrentText(transitionName.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_defaultTransitions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->defaultTransitions->item(idx);

	QString sceneTransition = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->defaultSceneTransitions)
	{
		if (sceneTransition.compare(s.sceneTransitionStr.c_str()) == 0)
		{
			string scene = GetWeakSourceName(s.scene);
			string transitionName = GetWeakSourceName(s.transition);
			ui->defaultTransitionsScene->setCurrentText(scene.c_str());
			ui->defaultTransitionsTransitions->setCurrentText(transitionName.c_str());
			break;
		}
	}
}


int SceneSwitcher::IgnoreIdleWindowsFindByData(const QString& window)
{
	int count = ui->ignoreIdleWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->ignoreIdleWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window)
		{
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_close_clicked()
{
	done(0);
}

void SceneSwitcher::on_add_clicked()
{
	QString sceneName = ui->scenes->currentText();
	QString windowName = ui->windows->currentText();
	QString transitionName = ui->transitions->currentText();
	bool fullscreen = ui->fullscreenCheckBox->isChecked();

	if (windowName.isEmpty() || sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(windowName);

	QString text = MakeSwitchName(sceneName, windowName, transitionName, fullscreen);

	int idx = FindByData(windowName);

	if (idx == -1)
	{
		lock_guard<mutex> lock(switcher->m);
		switcher->switches.emplace_back(
			source, windowName.toUtf8().constData(), transition, fullscreen);

		QListWidgetItem* item = new QListWidgetItem(text, ui->switches);
		item->setData(Qt::UserRole, v);
	}
	else
	{
		QListWidgetItem* item = ui->switches->item(idx);
		item->setText(text);

		string window = windowName.toUtf8().constData();

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->switches)
			{
				if (s.window == window)
				{
					s.scene = source;
					s.transition = transition;
					s.fullscreen = fullscreen;
					break;
				}
			}
		}

		ui->switches->sortItems();
	}
}

void SceneSwitcher::on_remove_clicked()
{
	QListWidgetItem* item = ui->switches->currentItem();
	if (!item)
		return;

	string window = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->switches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.window == window)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_screenRegionAdd_clicked()
{
	QString sceneName = ui->screenRegionScenes->currentText();
	QString transitionName = ui->screenRegionsTransitions->currentText();

	if (sceneName.isEmpty())
		return;

	int minX = ui->screenRegionMinX->value();
	int minY = ui->screenRegionMinY->value();
	int maxX = ui->screenRegionMaxX->value();
	int maxY = ui->screenRegionMaxY->value();

	string regionStr = to_string(minX) + ", " + to_string(minY) + " x " + to_string(maxX) + ", "
		+ to_string(maxY);
	QString region = QString::fromStdString(regionStr);

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(region);

	QString text = MakeScreenRegionSwitchName(sceneName, transitionName, minX, minY, maxX, maxY);

	int idx = ScreenRegionFindByData(region);

	if (idx == -1)
	{
		QListWidgetItem* item = new QListWidgetItem(text, ui->screenRegions);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->screenRegionSwitches.emplace_back(
			source, transition, minX, minY, maxX, maxY, regionStr);
	}
	else
	{
		QListWidgetItem* item = ui->screenRegions->item(idx);
		item->setText(text);

		string curRegion = region.toUtf8().constData();

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->screenRegionSwitches)
			{
				if (s.regionStr == curRegion)
				{
					s.scene = source;
					s.transition = transition;
					break;
				}
			}
		}

		ui->screenRegions->sortItems();
	}
}

void SceneSwitcher::on_screenRegionRemove_clicked()
{
	QListWidgetItem* item = ui->screenRegions->currentItem();
	if (!item)
		return;

	string region = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->screenRegionSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.regionStr == region)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_pauseScenesAdd_clicked()
{
	QString sceneName = ui->pauseScenesScenes->currentText();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	QVariant v = QVariant::fromValue(sceneName);

	QList<QListWidgetItem*> items = ui->pauseScenes->findItems(sceneName, Qt::MatchExactly);

	if (items.size() == 0)
	{
		QListWidgetItem* item = new QListWidgetItem(sceneName, ui->pauseScenes);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->pauseScenesSwitches.emplace_back(source);
		ui->pauseScenes->sortItems();
	}
}

void SceneSwitcher::on_pauseScenesRemove_clicked()
{
	QListWidgetItem* item = ui->pauseScenes->currentItem();
	if (!item)
		return;

	QString pauseScene = item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->pauseScenesSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s == GetWeakSourceByQString(pauseScene))
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_pauseWindowsAdd_clicked()
{
	QString windowName = ui->pauseWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem*> items = ui->pauseWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0)
	{
		QListWidgetItem* item = new QListWidgetItem(windowName, ui->pauseWindows);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->pauseWindowsSwitches.emplace_back(windowName.toUtf8().constData());
		ui->pauseWindows->sortItems();
	}
}

void SceneSwitcher::on_pauseWindowsRemove_clicked()
{
	QListWidgetItem* item = ui->pauseWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->pauseWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s == windowName.toUtf8().constData())
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_ignoreWindowsAdd_clicked()
{
	QString windowName = ui->ignoreWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem*> items = ui->ignoreWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0)
	{
		QListWidgetItem* item = new QListWidgetItem(windowName, ui->ignoreWindows);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->ignoreWindowsSwitches.emplace_back(windowName.toUtf8().constData());
		ui->ignoreWindows->sortItems();
	}
}

void SceneSwitcher::on_ignoreWindowsRemove_clicked()
{
	QListWidgetItem* item = ui->ignoreWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->ignoreWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s == windowName.toUtf8().constData())
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_sceneRoundTripAdd_clicked()
{
	QString scene1Name = ui->sceneRoundTripScenes1->currentText();
	QString scene2Name = ui->sceneRoundTripScenes2->currentText();
	QString transitionName = ui->sceneRoundTripTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	int delay = ui->sceneRoundTripSpinBox->value();

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeSceneRoundTripSwitchName(scene1Name, scene2Name, transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneRoundTripFindByData(scene1Name);

	if (idx == -1)
	{
		QListWidgetItem* item = new QListWidgetItem(text, ui->sceneRoundTrips);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->sceneRoundTripSwitches.emplace_back(
			source1, source2, transition, delay, text.toUtf8().constData());
	}
	else
	{
		QListWidgetItem* item = ui->sceneRoundTrips->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->sceneRoundTripSwitches)
			{
				if (s.scene1 == source1 && s.scene2 == source2)
				{
					s.delay = delay;
					s.transition = transition;
					s.sceneRoundTripStr = text.toUtf8().constData();
					break;
				}
			}
		}

		ui->sceneRoundTrips->sortItems();
	}
}

void SceneSwitcher::on_sceneRoundTripRemove_clicked()
{
	QListWidgetItem* item = ui->sceneRoundTrips->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->sceneRoundTripSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.sceneRoundTripStr == text)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_autoStopSceneCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (!state)
	{
		ui->autoStopScenes->setDisabled(true);
		switcher->autoStopEnable = false;
	}
	else
	{
		ui->autoStopScenes->setDisabled(false);
		switcher->autoStopEnable = true;
	}
}

void SceneSwitcher::UpdateAutoStopScene(const QString& name)
{
	obs_source_t* scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t* ws = obs_source_get_weak_source(scene);

	switcher->autoStopScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_autoStopScenes_currentTextChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateAutoStopScene(text);
}

void SceneSwitcher::on_sceneRoundTripSave_clicked()
{
	QString directory = QFileDialog::getSaveFileName(
		this, tr("Save Scene Round Trip to file ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
	{
		QFile file(directory);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
			return;
		QTextStream out(&file);
		for (SceneRoundTripSwitch s : switcher->sceneRoundTripSwitches)
		{
			out << QString::fromStdString(GetWeakSourceName(s.scene1)) << "\n";
			out << QString::fromStdString(GetWeakSourceName(s.scene2)) << "\n";
			out << s.delay << "\n";
			out << QString::fromStdString(s.sceneRoundTripStr) << "\n";
			out << QString::fromStdString(GetWeakSourceName(s.transition)) << "\n";
		}
	}
}

void SceneSwitcher::on_sceneRoundTripLoad_clicked()
{
	lock_guard<mutex> lock(switcher->m);

	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read Scene Round Trip from ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
	{
		QFile file(directory);
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;

		QTextStream in(&file);
		vector<QString> lines;

		vector<SceneRoundTripSwitch> newSceneRoundTripSwitch;

		while (!in.atEnd())
		{
			QString line = in.readLine();
			lines.push_back(line);
			if (lines.size() == 5)
			{
				OBSWeakSource scene1 = GetWeakSourceByQString(lines[0]);
				OBSWeakSource scene2 = GetWeakSourceByQString(lines[1]);
				OBSWeakSource transition = GetWeakTransitionByQString(lines[4]);

				if (WeakSourceValid(scene1) && WeakSourceValid(scene2)
					&& WeakSourceValid(transition))
				{
					newSceneRoundTripSwitch.emplace_back(SceneRoundTripSwitch(
						GetWeakSourceByQString(lines[0]),
						GetWeakSourceByQString(lines[1]),
						GetWeakTransitionByQString(lines[4]),
						lines[2].toInt(),
						lines[3].toStdString()));
				}
				lines.clear();
			}
		}
		//unvalid amount of lines in file or nothing valid read
		if (lines.size() != 0 || newSceneRoundTripSwitch.size() == 0)
			return;

		switcher->sceneRoundTripSwitches.clear();
		ui->sceneRoundTrips->clear();
		switcher->sceneRoundTripSwitches = newSceneRoundTripSwitch;
		for (SceneRoundTripSwitch s : switcher->sceneRoundTripSwitches)
		{
			QListWidgetItem* item = new QListWidgetItem(QString::fromStdString(s.sceneRoundTripStr), ui->sceneRoundTrips);
			item->setData(Qt::UserRole, QString::fromStdString(s.sceneRoundTripStr));
		}
	}
}

void SceneSwitcher::on_transitionsAdd_clicked()
{
	QString scene1Name = ui->transitionsScene1->currentText();
	QString scene2Name = ui->transitionsScene2->currentText();
	QString transitionName = ui->transitionsTransitions->currentText();

	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeSceneTransitionName(scene1Name, scene2Name, transitionName);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneTransitionsFindByData(scene1Name, scene2Name);

	if (idx == -1)
	{
		QListWidgetItem* item = new QListWidgetItem(text, ui->sceneTransitions);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->sceneTransitions.emplace_back(
			source1, source2, transition, text.toUtf8().constData());
	}
	else
	{
		QListWidgetItem* item = ui->sceneTransitions->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->sceneTransitions)
			{
				if (s.scene1 == source1 && s.scene2 == source2)
				{
					s.transition = transition;
					s.sceneTransitionStr = text.toUtf8().constData();
					break;
				}
			}
		}

		ui->sceneTransitions->sortItems();
	}
}

void SceneSwitcher::on_transitionsRemove_clicked()
{
	QListWidgetItem* item = ui->sceneTransitions->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->sceneTransitions;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.sceneTransitionStr == text)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_defaultTransitionsAdd_clicked()
{
	QString sceneName = ui->defaultTransitionsScene->currentText();
	QString transitionName = ui->defaultTransitionsTransitions->currentText();

	if (sceneName.isEmpty() || transitionName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeDefaultSceneTransitionName(sceneName, transitionName);
	QVariant v = QVariant::fromValue(text);

	int idx = DefaultTransitionsFindByData(sceneName);

	if (idx == -1)
	{
		QListWidgetItem* item = new QListWidgetItem(text, ui->defaultTransitions);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->defaultSceneTransitions.emplace_back(
			source, transition, text.toUtf8().constData());
	}
	else
	{
		QListWidgetItem* item = ui->defaultTransitions->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->defaultSceneTransitions)
			{
				if (s.scene == source)
				{
					s.transition = transition;
					s.sceneTransitionStr = text.toUtf8().constData();
					break;
				}
			}
		}

		ui->defaultTransitions->sortItems();
	}
}

void SceneSwitcher::on_defaultTransitionsRemove_clicked()
{
	QListWidgetItem* item = ui->defaultTransitions->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->defaultSceneTransitions;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.sceneTransitionStr == text)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

int SceneSwitcher::executableFindByData(const QString& exe)
{
	int count = ui->executables->count();

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->executables->item(i);
		QString itemExe = item->data(Qt::UserRole).toString();

		if (itemExe == exe)
			return i;
	}

	return -1;
}


void SceneSwitcher::on_executables_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->executables->item(idx);

	QString exec = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->executableSwitches)
	{
		if (exec.compare(s.mExe) == 0)
		{
			QString sceneName = GetWeakSourceName(s.mScene).c_str();
			QString transitionName = GetWeakSourceName(s.mTransition).c_str();
			ui->executableScenes->setCurrentText(sceneName);
			ui->executable->setCurrentText(exec);
			ui->executableTransitions->setCurrentText(transitionName);
			ui->requiresFocusCheckBox->setChecked(s.mInFocus);
			break;
		}
	}
}


void SceneSwitcher::on_executableAdd_clicked()
{
	QString sceneName = ui->executableScenes->currentText();
	QString exeName = ui->executable->currentText();
	QString transitionName = ui->executableTransitions->currentText();
	bool inFocus = ui->requiresFocusCheckBox->isChecked();

	if (exeName.isEmpty() || sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(exeName);

	QString text = MakeSwitchNameExecutable(sceneName, exeName, transitionName, inFocus);

	int idx = executableFindByData(exeName);

	if (idx == -1)
	{
		lock_guard<mutex> lock(switcher->m);
		switcher->executableSwitches.emplace_back(
			source, transition, exeName.toUtf8().constData(), inFocus);

		QListWidgetItem* item = new QListWidgetItem(text, ui->executables);
		item->setData(Qt::UserRole, v);
	}
	else
	{
		QListWidgetItem* item = ui->executables->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->executableSwitches)
			{
				if (s.mExe == exeName)
				{
					s.mScene = source;
					s.mTransition = transition;
					s.mInFocus = inFocus;
					break;
				}
			}
		}

		ui->executables->sortItems();
	}
}


void SceneSwitcher::on_executableRemove_clicked()
{
	QListWidgetItem* item = ui->executables->currentItem();
	if (!item)
		return;

	QString exe = item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->executableSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.mExe == exe)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_idleCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (!state)
	{
		ui->idleScenes->setDisabled(true);
		ui->idleSpinBox->setDisabled(true);
		ui->idleTransitions->setDisabled(true);

		switcher->idleData.idleEnable = false;
	}
	else
	{
		ui->idleScenes->setDisabled(false);
		ui->idleSpinBox->setDisabled(false);
		ui->idleTransitions->setDisabled(false);

		switcher->idleData.idleEnable = true;
	}
}


void SceneSwitcher::UpdateIdleDataTransition(const QString& name)
{
	obs_weak_source_t* transition = GetWeakTransitionByQString(name);
	switcher->idleData.transition = transition;
}

void SceneSwitcher::UpdateIdleDataScene(const QString& name)
{
	obs_source_t* scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t* ws = obs_source_get_weak_source(scene);

	switcher->idleData.scene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_idleTransitions_currentTextChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateIdleDataTransition(text);
}

void SceneSwitcher::on_idleScenes_currentTextChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateIdleDataScene(text);
}

void SceneSwitcher::on_idleSpinBox_valueChanged(int i)
{
	if (loading)
		return;
	lock_guard<mutex> lock(switcher->m);
	switcher->idleData.time = i;
}

void SceneSwitcher::on_ignoreIdleWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->ignoreIdleWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& w : switcher->ignoreIdleWindows)
	{
		if (window.compare(w.c_str()) == 0)
		{
			ui->ignoreIdleWindowsWindows->setCurrentText(w.c_str());
			break;
		}
	}
}

void SceneSwitcher::on_ignoreIdleAdd_clicked()
{
	QString windowName = ui->ignoreIdleWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem*> items = ui->ignoreIdleWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0)
	{
		QListWidgetItem* item = new QListWidgetItem(windowName, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->ignoreIdleWindows.emplace_back(windowName.toUtf8().constData());
		ui->ignoreIdleWindows->sortItems();
	}
}

void SceneSwitcher::on_ignoreIdleRemove_clicked()
{
	QListWidgetItem* item = ui->ignoreIdleWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& windows = switcher->ignoreIdleWindows;

		for (auto it = windows.begin(); it != windows.end(); ++it)
		{
			auto& s = *it;

			if (s == windowName.toUtf8().constData())
			{
				windows.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_browseButton_clicked()
{
	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to write to ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
		ui->writePathLineEdit->setText(directory);
}

void SceneSwitcher::on_readFileCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (!state)
	{
		ui->browseButton_2->setDisabled(true);
		ui->readPathLineEdit->setDisabled(true);
		switcher->fileIO.readEnabled = false;
	}
	else
	{
		ui->browseButton_2->setDisabled(false);
		ui->readPathLineEdit->setDisabled(false);
		switcher->fileIO.readEnabled = true;
	}
}

void SceneSwitcher::on_readPathLineEdit_textChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (text.isEmpty())
	{
		switcher->fileIO.readEnabled = false;
		return;
	}
	switcher->fileIO.readEnabled = true;
	switcher->fileIO.readPath = text.toUtf8().constData();
}

void SceneSwitcher::on_writePathLineEdit_textChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	if (text.isEmpty())
	{
		switcher->fileIO.writeEnabled = false;
		return;
	}
	switcher->fileIO.writeEnabled = true;
	switcher->fileIO.writePath = text.toUtf8().constData();
}

void SceneSwitcher::on_browseButton_2_clicked()
{
	QString directory = QFileDialog::getOpenFileName(
		this, tr("Select a file to read from ..."), QDir::currentPath(), tr("Text files (*.txt)"));
	if (!directory.isEmpty())
		ui->readPathLineEdit->setText(directory);
}

void SceneSwitcher::on_startAtLaunch_toggled(bool value)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->startAtLaunch = value;
}

void SceneSwitcher::UpdateNonMatchingScene(const QString& name)
{
	obs_source_t* scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t* ws = obs_source_get_weak_source(scene);

	switcher->nonMatchingScene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_noMatchDontSwitch_clicked()
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->switchIfNotMatching = false;
}

void SceneSwitcher::on_noMatchSwitch_clicked()
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->switchIfNotMatching = true;
	UpdateNonMatchingScene(ui->noMatchSwitchScene->currentText());
}

void SceneSwitcher::on_noMatchSwitchScene_currentTextChanged(const QString& text)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	UpdateNonMatchingScene(text);
}

void SceneSwitcher::on_checkInterval_valueChanged(int value)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->interval = value;
}

void SceneSwitcher::SetStarted()
{
	ui->toggleStartButton->setText(obs_module_text("Stop"));
	ui->pluginRunningText->setText(obs_module_text("Active"));
}

void SceneSwitcher::SetStopped()
{
	ui->toggleStartButton->setText(obs_module_text("Start"));
	ui->pluginRunningText->setText(obs_module_text("Inactive"));
}

void SceneSwitcher::on_toggleStartButton_clicked()
{
	if (switcher->th.joinable())
	{
		switcher->Stop();
		SetStopped();
	}
	else
	{
		switcher->Start();
		SetStarted();
	}
}

void SceneSwitcher::updateScreenRegionCursorPos()
{
	pair<int, int> position = getCursorPos();
	ui->cursorXPosition->setText(QString::number(position.first));
	;
	ui->cursorYPosition->setText(QString::number(position.second));
}

// TODO rename the save values (clears settings!)
static void SaveSceneSwitcher(obs_data_t* save_data, bool saving, void*)
{
	if (saving)
	{
		lock_guard<mutex> lock(switcher->m);
		obs_data_t* obj = obs_data_create();
		obs_data_array_t* array = obs_data_array_create();
		obs_data_array_t* screenRegionArray = obs_data_array_create();
		obs_data_array_t* pauseScenesArray = obs_data_array_create();
		obs_data_array_t* pauseWindowsArray = obs_data_array_create();
		obs_data_array_t* ignoreWindowsArray = obs_data_array_create();
		obs_data_array_t* sceneRoundTripArray = obs_data_array_create();
		obs_data_array_t* sceneTransitionsArray = obs_data_array_create();
		obs_data_array_t* defaultTransitionsArray = obs_data_array_create();
		obs_data_array_t* ignoreIdleWindowsArray = obs_data_array_create();

		switcher->Prune();

		for (SceneSwitch& s : switcher->switches)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source = obs_weak_source_get_source(s.scene);
			obs_source_t* transition = obs_weak_source_get_source(s.transition);
			if (source)
			{
				const char* sceneName = obs_source_get_name(source);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene", sceneName);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_string(array_obj, "window_title", s.window.c_str());
				obs_data_set_bool(array_obj, "fullscreen", s.fullscreen);
				obs_data_array_push_back(array, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (ScreenRegionSwitch& s : switcher->screenRegionSwitches)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source = obs_weak_source_get_source(s.scene);
			obs_source_t* transition = obs_weak_source_get_source(s.transition);
			if (source)
			{
				const char* sceneName = obs_source_get_name(source);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "screenRegionScene", sceneName);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_int(array_obj, "minX", s.minX);
				obs_data_set_int(array_obj, "minY", s.minY);
				obs_data_set_int(array_obj, "maxX", s.maxX);
				obs_data_set_int(array_obj, "maxY", s.maxY);
				obs_data_set_string(array_obj, "screenRegionStr", s.regionStr.c_str());
				obs_data_array_push_back(screenRegionArray, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (OBSWeakSource& scene : switcher->pauseScenesSwitches)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source = obs_weak_source_get_source(scene);
			if (source)
			{
				const char* n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "pauseScene", n);
				obs_data_array_push_back(pauseScenesArray, array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		for (string& window : switcher->pauseWindowsSwitches)
		{
			obs_data_t* array_obj = obs_data_create();
			obs_data_set_string(array_obj, "pauseWindow", window.c_str());
			obs_data_array_push_back(pauseWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (string& window : switcher->ignoreWindowsSwitches)
		{
			obs_data_t* array_obj = obs_data_create();
			obs_data_set_string(array_obj, "ignoreWindow", window.c_str());
			obs_data_array_push_back(ignoreWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (SceneRoundTripSwitch& s : switcher->sceneRoundTripSwitches)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source1 = obs_weak_source_get_source(s.scene1);
			obs_source_t* source2 = obs_weak_source_get_source(s.scene2);
			obs_source_t* transition = obs_weak_source_get_source(s.transition);
			if (source1 && source2)
			{
				const char* sceneName1 = obs_source_get_name(source1);
				const char* sceneName2 = obs_source_get_name(source2);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "sceneRoundTripScene1", sceneName1);
				obs_data_set_string(array_obj, "sceneRoundTripScene2", sceneName2);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_int(array_obj, "sceneRoundTripDelay", s.delay);
				obs_data_set_string(array_obj, "sceneRoundTripStr", s.sceneRoundTripStr.c_str());
				obs_data_array_push_back(sceneRoundTripArray, array_obj);
				obs_source_release(source1);
				obs_source_release(source2);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (SceneTransition& s : switcher->sceneTransitions)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source1 = obs_weak_source_get_source(s.scene1);
			obs_source_t* source2 = obs_weak_source_get_source(s.scene2);
			obs_source_t* transition = obs_weak_source_get_source(s.transition);
			if (source1 && source2)
			{
				const char* sceneName1 = obs_source_get_name(source1);
				const char* sceneName2 = obs_source_get_name(source2);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "Scene1", sceneName1);
				obs_data_set_string(array_obj, "Scene2", sceneName2);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_string(array_obj, "Str", s.sceneTransitionStr.c_str());
				obs_data_array_push_back(sceneTransitionsArray, array_obj);
				obs_source_release(source1);
				obs_source_release(source2);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (DefaultSceneTransition& s : switcher->defaultSceneTransitions)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source = obs_weak_source_get_source(s.scene);
			obs_source_t* transition = obs_weak_source_get_source(s.transition);
			if (source)
			{
				const char* sceneName = obs_source_get_name(source);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "Scene", sceneName);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_string(array_obj, "Str", s.sceneTransitionStr.c_str());
				obs_data_array_push_back(defaultTransitionsArray, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		obs_data_array_t* executableArray = obs_data_array_create();

		for (ExecutableSceneSwitch& s : switcher->executableSwitches)
		{
			obs_data_t* array_obj = obs_data_create();

			obs_source_t* source = obs_weak_source_get_source(s.mScene);
			obs_source_t* transition = obs_weak_source_get_source(s.mTransition);

			if (source)
			{
				const char* sceneName = obs_source_get_name(source);
				const char* transitionName = obs_source_get_name(transition);
				obs_data_set_string(array_obj, "scene", sceneName);
				obs_data_set_string(array_obj, "transition", transitionName);
				obs_data_set_string(array_obj, "exefile", s.mExe.toUtf8());
				obs_data_set_bool(array_obj, "infocus", s.mInFocus);
				obs_data_array_push_back(executableArray, array_obj);
				obs_source_release(source);
				obs_source_release(transition);
			}

			obs_data_release(array_obj);
		}

		for (string& window : switcher->ignoreIdleWindows)
		{
			obs_data_t* array_obj = obs_data_create();
			obs_data_set_string(array_obj, "window", window.c_str());
			obs_data_array_push_back(ignoreIdleWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		string nonMatchingSceneName = GetWeakSourceName(switcher->nonMatchingScene);

		obs_data_set_int(obj, "interval", switcher->interval);
		obs_data_set_string(obj, "non_matching_scene", nonMatchingSceneName.c_str());
		obs_data_set_bool(obj, "switch_if_not_matching", switcher->switchIfNotMatching);
		obs_data_set_bool(obj, "active", switcher->th.joinable());

		obs_data_set_array(obj, "switches", array);
		obs_data_set_array(obj, "screenRegion", screenRegionArray);
		obs_data_set_array(obj, "pauseScenes", pauseScenesArray);
		obs_data_set_array(obj, "pauseWindows", pauseWindowsArray);
		obs_data_set_array(obj, "ignoreWindows", ignoreWindowsArray);
		obs_data_set_array(obj, "sceneRoundTrip", sceneRoundTripArray);
		obs_data_set_array(obj, "sceneTransitions", sceneTransitionsArray);
		obs_data_set_array(obj, "defaultTransitions", defaultTransitionsArray);
		obs_data_set_array(obj, "executableSwitches", executableArray);
		obs_data_set_array(obj, "ignoreIdleWindows", ignoreIdleWindowsArray);


		string autoStopSceneName = GetWeakSourceName(switcher->autoStopScene);
		obs_data_set_bool(obj, "autoStopEnable", switcher->autoStopEnable);
		obs_data_set_string(obj, "autoStopSceneName", autoStopSceneName.c_str());

		string idleSceneName = GetWeakSourceName(switcher->idleData.scene);
		string idleTransitionName = GetWeakSourceName(switcher->idleData.transition);
		obs_data_set_bool(obj, "idleEnable", switcher->idleData.idleEnable);
		obs_data_set_string(obj, "idleSceneName", idleSceneName.c_str());
		obs_data_set_string(obj, "idleTransitionName", idleTransitionName.c_str());
		obs_data_set_int(obj, "idleTime", switcher->idleData.time);

		obs_data_set_bool(obj, "readEnabled", switcher->fileIO.readEnabled);
		obs_data_set_string(obj, "readPath", switcher->fileIO.readPath.c_str());
		obs_data_set_bool(obj, "writeEnabled", switcher->fileIO.writeEnabled);
		obs_data_set_string(obj, "writePath", switcher->fileIO.writePath.c_str());

		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);

		obs_data_array_release(array);
		obs_data_array_release(screenRegionArray);
		obs_data_array_release(pauseScenesArray);
		obs_data_array_release(pauseWindowsArray);
		obs_data_array_release(ignoreWindowsArray);
		obs_data_array_release(sceneRoundTripArray);
		obs_data_array_release(sceneTransitionsArray);
		obs_data_array_release(defaultTransitionsArray);
		obs_data_array_release(executableArray);
		obs_data_array_release(ignoreIdleWindowsArray);

		obs_data_release(obj);
	}
	else
	{
		switcher->m.lock();

		obs_data_t* obj = obs_data_get_obj(save_data, "advanced-scene-switcher");
		obs_data_array_t* array = obs_data_get_array(obj, "switches");
		obs_data_array_t* screenRegionArray = obs_data_get_array(obj, "screenRegion");
		obs_data_array_t* pauseScenesArray = obs_data_get_array(obj, "pauseScenes");
		obs_data_array_t* pauseWindowsArray = obs_data_get_array(obj, "pauseWindows");
		obs_data_array_t* ignoreWindowsArray = obs_data_get_array(obj, "ignoreWindows");
		obs_data_array_t* sceneRoundTripArray = obs_data_get_array(obj, "sceneRoundTrip");
		obs_data_array_t* sceneTransitionsArray = obs_data_get_array(obj, "sceneTransitions");
		obs_data_array_t* defaultTransitionsArray = obs_data_get_array(obj, "defaultTransitions");
		obs_data_array_t* executableArray = obs_data_get_array(obj, "executableSwitches");
		obs_data_array_t* ignoreIdleWindowsArray = obs_data_get_array(obj, "ignoreIdleWindows");

		if (!obj)
			obj = obs_data_create();

		obs_data_set_default_int(obj, "interval", DEFAULT_INTERVAL);

		switcher->interval = obs_data_get_int(obj, "interval");
		switcher->switchIfNotMatching = obs_data_get_bool(obj, "switch_if_not_matching");
		string nonMatchingScene = obs_data_get_string(obj, "non_matching_scene");
		bool active = obs_data_get_bool(obj, "active");

		switcher->nonMatchingScene = GetWeakSourceByName(nonMatchingScene.c_str());

		switcher->switches.clear();
		size_t count = obs_data_array_count(array);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(array, i);

			const char* scene = obs_data_get_string(array_obj, "scene");
			const char* transition = obs_data_get_string(array_obj, "transition");
			const char* window = obs_data_get_string(array_obj, "window_title");
			bool fullscreen = obs_data_get_bool(array_obj, "fullscreen");

			switcher->switches.emplace_back(GetWeakSourceByName(scene), window,
				GetWeakTransitionByName(transition), fullscreen);

			obs_data_release(array_obj);
		}

		switcher->screenRegionSwitches.clear();
		count = obs_data_array_count(screenRegionArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(screenRegionArray, i);

			const char* scene = obs_data_get_string(array_obj, "screenRegionScene");
			const char* transition = obs_data_get_string(array_obj, "transition");
			int minX = obs_data_get_int(array_obj, "minX");
			int minY = obs_data_get_int(array_obj, "minY");
			int maxX = obs_data_get_int(array_obj, "maxX");
			int maxY = obs_data_get_int(array_obj, "maxY");
			string regionStr = obs_data_get_string(array_obj, "screenRegionStr");

			switcher->screenRegionSwitches.emplace_back(GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition), minX, minY, maxX, maxY, regionStr);

			obs_data_release(array_obj);
		}

		switcher->pauseScenesSwitches.clear();
		count = obs_data_array_count(pauseScenesArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(pauseScenesArray, i);

			const char* scene = obs_data_get_string(array_obj, "pauseScene");

			switcher->pauseScenesSwitches.emplace_back(GetWeakSourceByName(scene));

			obs_data_release(array_obj);
		}

		switcher->pauseWindowsSwitches.clear();
		count = obs_data_array_count(pauseWindowsArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(pauseWindowsArray, i);

			const char* window = obs_data_get_string(array_obj, "pauseWindow");

			switcher->pauseWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		obs_data_array_release(pauseWindowsArray);

		switcher->ignoreWindowsSwitches.clear();
		count = obs_data_array_count(ignoreWindowsArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(ignoreWindowsArray, i);

			const char* window = obs_data_get_string(array_obj, "ignoreWindow");

			switcher->ignoreWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		switcher->sceneRoundTripSwitches.clear();
		count = obs_data_array_count(sceneRoundTripArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(sceneRoundTripArray, i);

			const char* scene1 = obs_data_get_string(array_obj, "sceneRoundTripScene1");
			const char* scene2 = obs_data_get_string(array_obj, "sceneRoundTripScene2");
			const char* transition = obs_data_get_string(array_obj, "transition");
			int delay = obs_data_get_int(array_obj, "sceneRoundTripDelay");
			const char* sceneRoundTripStr = obs_data_get_string(array_obj, "sceneRoundTripStr");

			switcher->sceneRoundTripSwitches.emplace_back(GetWeakSourceByName(scene1),
				GetWeakSourceByName(scene2), GetWeakTransitionByName(transition), delay,
				sceneRoundTripStr);

			obs_data_release(array_obj);
		}

		switcher->sceneTransitions.clear();
		count = obs_data_array_count(sceneTransitionsArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(sceneTransitionsArray, i);

			const char* scene1 = obs_data_get_string(array_obj, "Scene1");
			const char* scene2 = obs_data_get_string(array_obj, "Scene2");
			const char* transition = obs_data_get_string(array_obj, "transition");
			const char* sceneTransitionsStr = obs_data_get_string(array_obj, "Str");

			switcher->sceneTransitions.emplace_back(GetWeakSourceByName(scene1),
				GetWeakSourceByName(scene2), GetWeakTransitionByName(transition),
				sceneTransitionsStr);

			obs_data_release(array_obj);
		}

		switcher->defaultSceneTransitions.clear();
		count = obs_data_array_count(defaultTransitionsArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(defaultTransitionsArray, i);

			const char* scene = obs_data_get_string(array_obj, "Scene");
			const char* transition = obs_data_get_string(array_obj, "transition");
			const char* sceneTransitionsStr = obs_data_get_string(array_obj, "Str");

			switcher->defaultSceneTransitions.emplace_back(GetWeakSourceByName(scene),
				GetWeakTransitionByName(transition),
				sceneTransitionsStr);

			obs_data_release(array_obj);
		}

		switcher->executableSwitches.clear();
		count = obs_data_array_count(executableArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(executableArray, i);

			const char* scene = obs_data_get_string(array_obj, "scene");
			const char* transition = obs_data_get_string(array_obj, "transition");
			const char* exe = obs_data_get_string(array_obj, "exefile");
			bool infocus = obs_data_get_bool(array_obj, "infocus");

			switcher->executableSwitches.emplace_back(
				GetWeakSourceByName(scene), GetWeakTransitionByName(transition), exe, infocus);

			obs_data_release(array_obj);
		}

		switcher->ignoreIdleWindows.clear();
		count = obs_data_array_count(ignoreIdleWindowsArray);

		for (size_t i = 0; i < count; i++)
		{
			obs_data_t* array_obj = obs_data_array_item(ignoreIdleWindowsArray, i);

			const char* window = obs_data_get_string(array_obj, "window");

			switcher->ignoreIdleWindows.emplace_back(window);

			obs_data_release(array_obj);
		}

		string autoStopScene = obs_data_get_string(obj, "autoStopSceneName");
		switcher->autoStopEnable = obs_data_get_bool(obj, "autoStopEnable");
		switcher->autoStopScene = GetWeakSourceByName(autoStopScene.c_str());

		string idleSceneName = obs_data_get_string(obj, "idleSceneName");
		string idleTransitionName = obs_data_get_string(obj, "idleTransitionName");
		switcher->idleData.scene = GetWeakSourceByName(idleSceneName.c_str());
		switcher->idleData.transition = GetWeakTransitionByName(idleTransitionName.c_str());
		switcher->idleData.idleEnable = obs_data_get_bool(obj, "idleEnable");
		switcher->idleData.time = obs_data_get_int(obj, "idleTime");

		switcher->fileIO.readEnabled = obs_data_get_bool(obj, "readEnabled");
		switcher->fileIO.readPath = obs_data_get_string(obj, "readPath");
		switcher->fileIO.writeEnabled = obs_data_get_bool(obj, "writeEnabled");
		switcher->fileIO.writePath = obs_data_get_string(obj, "writePath");

		obs_data_array_release(array);
		obs_data_array_release(screenRegionArray);
		obs_data_array_release(pauseScenesArray);
		obs_data_array_release(ignoreWindowsArray);
		obs_data_array_release(sceneRoundTripArray);
		obs_data_array_release(sceneTransitionsArray);
		obs_data_array_release(defaultTransitionsArray);
		obs_data_array_release(executableArray);
		obs_data_array_release(ignoreIdleWindowsArray);

		obs_data_release(obj);

		switcher->m.unlock();

		if (active)
			switcher->Start();
		else
			switcher->Stop();
	}
}

static inline OBSWeakSource getNextTransition(OBSWeakSource scene1, OBSWeakSource scene2)
{
	OBSWeakSource ws;
	if (scene1 && scene2)
	{
		for (SceneTransition& t : switcher->sceneTransitions)
		{
			if (t.scene1 == scene1 && t.scene2 == scene2)
				ws = t.transition;
		}
	}
	return ws;
}

void SwitcherData::Thread()
{
	chrono::duration<long long, milli> duration = chrono::milliseconds(interval);
	string lastTitle;
	string title;

	while (true)
	{
		unique_lock<mutex> lock(waitMutex);
		unique_lock<mutex> transitionLock(transitionEndMutex);
		OBSWeakSource scene;
		OBSWeakSource transition;
		bool match = false;
		bool fullscreen = false;
		bool pause = false;
		bool sceneRoundTripActive = false;

		// Files
		// TODO add transitions to output file / read them in the input file
		obs_source_t* currentSource = obs_frontend_get_current_scene();

		if (fileIO.writeEnabled)
		{
			QFile file(QString::fromStdString(fileIO.writePath));
			if (file.open(QIODevice::WriteOnly))
			{
				const char* msg = obs_source_get_name(currentSource);
				file.write(msg, qstrlen(msg));
				file.close();
			}
		}

		obs_source_release(currentSource);

		cv.wait_for(lock, duration);

		{
			lock_guard<mutex> lock(threadEndMutex);
			if (switcher->stop)
			{
				break;
			}
		}

		currentSource = obs_frontend_get_current_scene();
		OBSWeakSource ws = obs_source_get_weak_source(currentSource);

		//Default transition change

		for (DefaultSceneTransition& s : defaultSceneTransitions)
		{
			if (s.scene == ws)
			{
				obs_source_t* transition = obs_weak_source_get_source(s.transition);
				if (transitionActive)
					transitionCv.wait(transitionLock);
				obs_frontend_set_current_transition(transition);
				obs_source_release(transition);
				break;
			}
		}

		//End default transition change

		if (fileIO.readEnabled)
		{
			QFile file(QString::fromStdString(fileIO.readPath));
			if (file.open(QIODevice::ReadOnly))
			{
				QTextStream in(&file);
				QString sceneStr = in.readLine();
				obs_source_t* scene = obs_get_source_by_name(sceneStr.toUtf8().constData());
				if (scene && scene != currentSource){
					obs_source_t* transition;
					OBSWeakSource sceneWs = obs_source_get_weak_source(scene);
					OBSWeakSource transitionWs = getNextTransition(ws, sceneWs);

					if (transitionWs){
						transition = obs_weak_source_get_source(transitionWs);
						if (transitionActive)
							transitionCv.wait(transitionLock);
						obs_frontend_set_current_transition(transition);
					}

					obs_frontend_set_current_scene(scene);

					obs_weak_source_release(transitionWs);
					obs_weak_source_release(sceneWs);
				}
				file.close();
				obs_source_release(scene);
			}
			obs_source_release(currentSource);
			obs_weak_source_release(ws);
			continue;
		}

		// End Files

		// Auto stop streaming/recording

		if (ws && autoStopScene == ws)
		{
			if (obs_frontend_streaming_active())
				obs_frontend_streaming_stop();
			if (obs_frontend_recording_active())
				obs_frontend_recording_stop();
		}
		obs_weak_source_release(ws);

		// End auto stop streaming/recording

		// Pause

		for (OBSWeakSource& s : pauseScenesSwitches)
		{
			OBSWeakSource ws = obs_source_get_weak_source(currentSource);
			if (s == ws)
			{
				pause = true;
				obs_weak_source_release(ws);
				break;
			}
			obs_weak_source_release(ws);
		}

		if (!pause)
		{
			GetCurrentWindowTitle(title);
			for (string& window : pauseWindowsSwitches)
			{
				if (window == title)
				{
					pause = true;
					break;
				}
			}
		}

		if (!pause)
		{
			GetCurrentWindowTitle(title);
			for (string& window : pauseWindowsSwitches)
			{
				try
				{
					bool matches = regex_match(title, regex(window));
					if (matches)
					{
						pause = true;
						break;
					}
				}
				catch (const regex_error&)
				{
				}
			}
		}

		if (pause)
		{
			obs_source_release(currentSource);
			continue;
		}

		// End Pause

		// Scene Round Trip

		for (SceneRoundTripSwitch& s : sceneRoundTripSwitches)
		{
			OBSWeakSource ws = obs_source_get_weak_source(currentSource);

			if (s.scene1 == ws)
			{
				sceneRoundTripActive = true;
				int dur = s.delay * 1000 - interval;
				if (dur > 30)
					cv.wait_for(lock, chrono::milliseconds(dur));
				else
					cv.wait_for(lock, chrono::milliseconds(30));

				obs_source_t* source = obs_weak_source_get_source(s.scene2);
				obs_source_t* currentSource2 = obs_frontend_get_current_scene();

				// don't switch if user changed scene manually
				if (currentSource == currentSource2)
				{
					obs_source_t* transition;
					OBSWeakSource transitionWs = getNextTransition(s.scene1, s.scene2);

					if (transitionWs)
						transition = obs_weak_source_get_source(transitionWs);
					else
						transition = obs_weak_source_get_source(s.transition);
					if (transitionActive)
						transitionCv.wait(transitionLock);
					obs_frontend_set_current_transition(transition);
					obs_frontend_set_current_scene(source);

					obs_source_release(transition);
				}

				obs_source_release(source);
				obs_weak_source_release(ws);
				obs_source_release(currentSource2);

				break;
			}
			obs_weak_source_release(ws);
		}

		obs_source_release(currentSource);

		if (sceneRoundTripActive)
		{
			continue;
		}

		// End Scene Round Trip

		duration = chrono::milliseconds(interval);

		GetCurrentWindowTitle(title);

		// Ignore Windows

		for (auto& window : ignoreWindowsSwitches)
		{
			if (window == title)
			{
				title = lastTitle;
				break;
			}
		}

		lastTitle = title;

		// End Ignore Windows

		switcher->Prune();

		// Idle switch

		if (!match && idleData.idleEnable)
		{
			bool ignoreIdle = false;
			GetCurrentWindowTitle(title);

			for (string& window : ignoreIdleWindows)
			{
				if (window == title)
				{
					ignoreIdle = true;
					break;
				}
			}

			if (!ignoreIdle)
			{
				for (string& window : ignoreIdleWindows)
				{
					try
					{
						bool matches = regex_match(title, regex(window));
						if (matches)
						{
							ignoreIdle = true;
							break;
						}
					}
					catch (const regex_error&)
					{
					}
				}
			}

			if (!ignoreIdle && secondsSinceLastInput() > idleData.time)
			{
				scene = idleData.scene;
				transition = idleData.transition;
				fullscreen = false;
				match = true;
			}
		}

		// End Idle switch

		// Regular switch

		if (!match){
			for (SceneSwitch& s : switches)
			{
				if (s.window == title)
				{
					match = true;
					scene = s.scene;
					transition = s.transition;
					fullscreen = s.fullscreen;
					break;
				}
			}
		}

		/* try regex */
		if (!match)
		{
			for (SceneSwitch& s : switches)
			{
				try
				{
					bool matches = regex_match(title, regex(s.window));
					if (matches)
					{
						match = true;
						scene = s.scene;
						transition = s.transition;
						fullscreen = s.fullscreen;
						break;
					}
				}
				catch (const regex_error&)
				{
				}
			}
		}

		// End regular switch

		// Executable switch
		QStringList runningProcesses;
		GetProcessList(runningProcesses);
		for (ExecutableSceneSwitch& s : executableSwitches)
		{
			if (runningProcesses.contains(s.mExe))
			{
				scene = s.mScene;
				transition = s.mTransition;
				match = !s.mInFocus || (s.mInFocus && isInFocus(s.mExe));
				break;
			}
		}
		// End executable switch

		// Screen Region

		if (!match)
		{
			pair<int, int> cursorPos = getCursorPos();
			int minRegionSize = 99999;

			for (auto& s : screenRegionSwitches)
			{
				if (cursorPos.first >= s.minX && cursorPos.second >= s.minY
					&& cursorPos.first <= s.maxX && cursorPos.second <= s.maxY)
				{
					int regionSize = (s.maxX - s.minX) + (s.maxY - s.minY);
					if (regionSize < minRegionSize)
					{
						match = true;
						scene = s.scene;
						transition = s.transition;
						minRegionSize = regionSize;
					}
				}
			}
		}

		// End Screen Region

		// Fullscreen

		match = match && (!fullscreen || (fullscreen && isFullscreen()));

		// End fullscreen

		// Backup Scene

		if (!match && switchIfNotMatching && nonMatchingScene)
		{
			match = true;
			scene = nonMatchingScene;
		}

		// End Backup Scene

		// Switch scene

		if (match)
		{
			obs_source_t* source = obs_weak_source_get_source(scene);
			obs_source_t* currentSource = obs_frontend_get_current_scene();

			if (source && source != currentSource)
			{
				obs_source_t* nextTransition;
				OBSWeakSource currentScene = obs_source_get_weak_source(currentSource);
				obs_weak_source_release(currentScene);
				OBSWeakSource nextTransitionWs = getNextTransition(currentScene, scene);

				if (nextTransitionWs)
				{
					obs_source_t* nextTransition = obs_weak_source_get_source(nextTransitionWs);
					if (transitionActive)
						transitionCv.wait(transitionLock);
					obs_frontend_set_current_transition(nextTransition);
					obs_source_release(nextTransition);
				}
				else if (transition)
				{
					obs_source_t* nextTransition = obs_weak_source_get_source(transition);
					if (transitionActive)
						transitionCv.wait(transitionLock);
					obs_frontend_set_current_transition(nextTransition);
					obs_source_release(nextTransition);
				}

				obs_frontend_set_current_scene(source);
			}
			obs_source_release(currentSource);
			obs_source_release(source);
		}

		// End switch scene
	}
}

void SwitcherData::Start()
{

	if (!th.joinable())
	{
		threadEndMutex.lock();
		stop = false;
		threadEndMutex.unlock();
		switcher->th = thread([]()
		{
			switcher->Thread();
		});
	}
}

void SwitcherData::Stop()
{
	if (th.joinable())
	{
		{
			lock_guard<mutex> lock(threadEndMutex);
			switcher->stop = true;
		}

		//LSKDJFKLSDFJSLDKJFLSDKJFLSDKJFSLDKFJSLDKJFSLDFKJSLDFJLKJAÄFKLJÖVOIJÖEOIJFÖLKAJOCIJWEOÖIHF
		//maybe it is somehting to with not properly freeing something??
		//deadlock???
		for (int i = 0; i < 100; i++){
			this_thread::sleep_for(chrono::milliseconds(100));
			transitionActive = false;
			transitionCv.notify_one();
			cv.notify_one();
		}
		cv.notify_one();
		transitionCv.notify_one();
		cv.notify_one();
		th.join();
	}
}

// HOTKEY

void startStopHotkeyFunc(void* data, obs_hotkey_id id, obs_hotkey_t* hotkey, bool pressed)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);

	if (pressed)
	{
		if (switcher->th.joinable())
			switcher->Stop();
		else
			switcher->Start();
	}

	obs_data_array* hotkeyData = obs_hotkey_save(id);

	if (hotkeyData != NULL)
	{
		char* path = obs_module_config_path("");
		ofstream file;
		file.open(string(path).append("hotkey.txt"), ofstream::trunc);
		if (file.is_open())
		{
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++)
			{
				obs_data_t* data = obs_data_array_item(hotkeyData, i);
				string temp = obs_data_get_json(data);
				obs_data_release(data);
				file << temp;
			}
			file.close();
		}
		bfree(path);
	}
	obs_data_array_release(hotkeyData);
}

string loadConfigFile(string filename)
{
	ifstream settingsFile;
	char* path = obs_module_config_path("");
	string value;

	settingsFile.open(string(path).append(filename));
	if (settingsFile.is_open())
	{
		settingsFile.seekg(0, ios::end);
		value.reserve(settingsFile.tellg());
		settingsFile.seekg(0, ios::beg);
		value.assign((istreambuf_iterator<char>(settingsFile)), istreambuf_iterator<char>());
		settingsFile.close();
	}
	bfree(path);
	return value;
}

void loadKeybinding(obs_hotkey_id hotkeyId)
{
	string bindings = loadConfigFile("hotkey.txt");
	if (!bindings.empty())
	{
		obs_data_array_t* hotkeyData = obs_data_array_create();
		obs_data_t* data = obs_data_create_from_json(bindings.c_str());
		obs_data_array_insert(hotkeyData, 0, data);
		obs_data_release(data);
		obs_hotkey_load(hotkeyId, hotkeyData);
		obs_data_array_release(hotkeyData);
	}
}

// END HOTKEY

extern "C" void FreeSceneSwitcher()
{
	delete switcher;
	switcher = nullptr;
}

static void OBSEvent(enum obs_frontend_event event, void* switcher)
{
	if (event == OBS_FRONTEND_EVENT_EXIT)
	{
		FreeSceneSwitcher();
	}

	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED)
	{
		SwitcherData* s = (SwitcherData*)switcher;
		s->stop ? s->transitionActive = false : s->transitionActive = true;
	}

	if (event == OBS_FRONTEND_EVENT_TRANSITION_STOPPED)
	{
		SwitcherData* s = (SwitcherData*)switcher;
		if (s->transitionActive)
		{
			/*this sleep seems to be necessary 
			since transitions can still not be completely stopped 
			even though we are in this event*/
			this_thread::sleep_for(chrono::milliseconds(100));
			s->transitionActive = false;
			s->transitionCv.notify_one();
		}

	}

	if (event == OBS_FRONTEND_EVENT_TRANSITION_CHANGED)
	{
		SwitcherData* s = (SwitcherData*)switcher;
		if (s->transitionActive)
		{
			s->transitionActive = false;
			s->transitionCv.notify_one();
		}

	}
}

extern "C" void InitSceneSwitcher()
{
	QAction* action
		= (QAction*)obs_frontend_add_tools_menu_qaction(obs_module_text("Advanced Scene Switcher"));

	switcher = new SwitcherData;

	auto cb = []()
	{
		obs_frontend_push_ui_translation(obs_module_get_string);

		QMainWindow* window = (QMainWindow*)obs_frontend_get_main_window();

		SceneSwitcher ss(window);
		ss.exec();

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, switcher);

	action->connect(action, &QAction::triggered, cb);

	char* path = obs_module_config_path("");
	QDir dir(path);
	if (!dir.exists())
	{
		dir.mkpath(".");
	}
	bfree(path);

	obs_hotkey_id pauseHotkeyId = obs_hotkey_register_frontend("startStopSwitcherHotkey",
		"Toggle Start/Stop for the Advanced Scene Switcher", startStopHotkeyFunc, NULL);
	loadKeybinding(pauseHotkeyId);
}
