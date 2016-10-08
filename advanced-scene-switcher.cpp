#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>
#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include "advanced-scene-switcher.hpp"

#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>
#include <fstream>
#include <boost/filesystem.hpp>

using namespace std;

#define DEFAULT_INTERVAL 300

struct SceneSwitch {
	OBSWeakSource scene;
	string window;
	bool fullscreen;

	inline SceneSwitch(OBSWeakSource scene_, const char *window_, bool fullscreen_)
		: scene(scene_), window(window_), fullscreen(fullscreen_)
	{
	}
};

struct ScreenRegionSwitch {
	OBSWeakSource scene;
	int minX, minY, maxX, maxY;
	string regionStr;

	inline ScreenRegionSwitch(OBSWeakSource scene_, int minX_, int minY_, int maxX_, int maxY_, string regionStr_)
		: scene(scene_), minX(minX_), minY(minY_), maxX(maxX_), maxY(maxY_), regionStr(regionStr_)
	{
	}
};

struct SceneRoundTripSwitch {
	OBSWeakSource scene1;
	OBSWeakSource scene2;
	int delay;
	string sceneRoundTripStr;

	inline SceneRoundTripSwitch(OBSWeakSource scene1_, OBSWeakSource scene2_, int delay_, string str)
		: scene1(scene1_), scene2(scene2_), delay(delay_), sceneRoundTripStr(str)
	{
	}
};


static inline bool WeakSourceValid(obs_weak_source_t *ws)
{
	obs_source_t *source = obs_weak_source_get_source(ws);
	if (source)
		obs_source_release(source);
	return !!source;
}

struct SwitcherData {
	thread th;
	condition_variable cv;
	mutex m;
	mutex threadEndMutex;
	mutex waitMutex;
	bool stop = true;
	//ThreadStopValue test;

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

	void Thread();
	void Start();
	void Stop();

	void Prune()
	{
		for (size_t i = 0; i < switches.size(); i++) {
			SceneSwitch &s = switches[i];
			if (!WeakSourceValid(s.scene))
				switches.erase(switches.begin() + i--);
		}

		if (nonMatchingScene && !WeakSourceValid(nonMatchingScene)) {
			switchIfNotMatching = false;
			nonMatchingScene = nullptr;
		}

		for (size_t i = 0; i < screenRegionSwitches.size(); i++) {
			ScreenRegionSwitch &s = screenRegionSwitches[i];
			if (!WeakSourceValid(s.scene))
				screenRegionSwitches.erase(screenRegionSwitches.begin() + i--);
		}

		for (size_t i = 0; i < pauseScenesSwitches.size(); i++) {
			OBSWeakSource &scene = pauseScenesSwitches[i];
			if (!WeakSourceValid(scene))
				pauseScenesSwitches.erase(pauseScenesSwitches.begin() + i--);
		}

		for (size_t i = 0; i < sceneRoundTripSwitches.size(); i++) {
			SceneRoundTripSwitch &s = sceneRoundTripSwitches[i];
			if (!WeakSourceValid(s.scene1) || !WeakSourceValid(s.scene2))
				sceneRoundTripSwitches.erase(sceneRoundTripSwitches.begin() + i--);
		}
	}

	inline ~SwitcherData()
	{
		Stop();
	}
};

static SwitcherData *switcher = nullptr;

static inline QString MakeSwitchName(const QString &scene,
	const QString &value, bool fullscreen)
{
	if (!fullscreen)
		return QStringLiteral("[") + scene + QStringLiteral("]: ") + value;
	return QStringLiteral("[") + scene + QStringLiteral("]: ") + value + QStringLiteral(" (only if window is fullscreen)");
}

static inline QString MakeScreenRegionSwitchName(const QString &scene,
	int minX, int minY, int maxX, int maxY)
{
	return QStringLiteral("[") + scene + QStringLiteral("]: ") + QString::number(minX) + QStringLiteral(", ") + QString::number(minY) + QStringLiteral(" x ") + QString::number(maxX) + QStringLiteral(", ") + QString::number(maxY);
}

static inline QString MakeSceneRoundTripSwitchName(const QString &scene1,
	const QString &scene2, int delay)
{
	return scene1 + QStringLiteral(" -> wait for ") + QString::number(delay) + QStringLiteral(" seconds -> ") + scene2;
}

static inline string GetWeakSourceName(obs_weak_source_t *weak_source)
{
	string name;

	obs_source_t *source = obs_weak_source_get_source(weak_source);
	if (source) {
		name = obs_source_get_name(source);
		obs_source_release(source);
	}

	return name;
}

static inline OBSWeakSource GetWeakSourceByName(const char *name)
{
	OBSWeakSource weak;
	obs_source_t *source = obs_get_source_by_name(name);
	if (source) {
		weak = obs_source_get_weak_source(source);
		obs_weak_source_release(weak);
		obs_source_release(source);
	}

	return weak;
}

static inline OBSWeakSource GetWeakSourceByQString(const QString &name)
{
	return GetWeakSourceByName(name.toUtf8().constData());
}

SceneSwitcher::SceneSwitcher(QWidget *parent)
	: QDialog(parent),
	ui(new Ui_SceneSwitcher)
{
	ui->setupUi(this);

	lock_guard<mutex> lock(switcher->m);

	switcher->Prune();

	BPtr<char*> scenes = obs_frontend_get_scene_names();
	char **temp = scenes;
	while (*temp) {
		const char *name = *temp;
		ui->scenes->addItem(name);
		ui->noMatchSwitchScene->addItem(name);
		ui->screenRegionScenes->addItem(name);
		ui->pauseScenesScenes->addItem(name);
		ui->sceneRoundTripScenes1->addItem(name);
		ui->sceneRoundTripScenes2->addItem(name);
		temp++;
	}

	if (switcher->switchIfNotMatching)
		ui->noMatchSwitch->setChecked(true);
	else
		ui->noMatchDontSwitch->setChecked(true);

	ui->noMatchSwitchScene->setCurrentText(
		GetWeakSourceName(switcher->nonMatchingScene).c_str());
	ui->checkInterval->setValue(switcher->interval);

	vector<string> windows;
	GetWindowList(windows);

	for (string &window : windows){
		ui->windows->addItem(window.c_str());
		ui->ignoreWindowsWindows->addItem(window.c_str());
		ui->pauseWindowsWindows->addItem(window.c_str());
	}

	for (auto &s : switcher->switches) {
		string sceneName = GetWeakSourceName(s.scene);
		QString text = MakeSwitchName(sceneName.c_str(),
			s.window.c_str(), s.fullscreen);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->switches);
		item->setData(Qt::UserRole, s.window.c_str());
	}

	for (auto &s : switcher->screenRegionSwitches) {
		string sceneName = GetWeakSourceName(s.scene);
		QString text = MakeScreenRegionSwitchName(sceneName.c_str(),
			s.minX, s.minY, s.maxX, s.maxY);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->screenRegions);
		item->setData(Qt::UserRole, s.regionStr.c_str());
	}

	for (auto &scene : switcher->pauseScenesSwitches) {
		string sceneName = GetWeakSourceName(scene);
		QString text = QString::fromStdString(sceneName);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->pauseScenes);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->pauseWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->pauseWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->ignoreWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->ignoreWindows);
		item->setData(Qt::UserRole, text);
	}

	for (auto &s : switcher->sceneRoundTripSwitches) {
		string sceneName1 = GetWeakSourceName(s.scene1);
		string sceneName2 = GetWeakSourceName(s.scene2);
		QString text = MakeSceneRoundTripSwitchName(sceneName1.c_str(),
			sceneName2.c_str(),
			s.delay);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->sceneRoundTrips);
		item->setData(Qt::UserRole, text);
	}

	if (switcher->th.joinable())
		SetStarted();
	else
		SetStopped();

	loading = false;
	//QObject::connect(&switcher->test, SIGNAL(switcher->test.valueChanged(bool)), this, SLOT(on_toggleStartButton_clicked()));
}

void SceneSwitcher::closeEvent(QCloseEvent*)
{
	obs_frontend_save();
}

int SceneSwitcher::FindByData(const QString &window)
{
	int count = ui->switches->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->switches->item(i);
		QString itemWindow =
			item->data(Qt::UserRole).toString();

		if (itemWindow == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::ScreenRegionFindByData(const QString &region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->screenRegions->item(i);
		QString itemRegion =
			item->data(Qt::UserRole).toString();

		if (itemRegion == region) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::PauseScenesFindByData(const QString &region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->screenRegions->item(i);
		QString itemRegion =
			item->data(Qt::UserRole).toString();

		if (itemRegion == region) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::PauseWindowsFindByData(const QString &region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->screenRegions->item(i);
		QString itemRegion =
			item->data(Qt::UserRole).toString();

		if (itemRegion == region) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::IgnoreWindowsFindByData(const QString &region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->screenRegions->item(i);
		QString itemRegion =
			item->data(Qt::UserRole).toString();

		if (itemRegion == region) {
			idx = i;
			break;
		}
	}

	return idx;
}

int SceneSwitcher::SceneRoundTripFindByData(const QString &scene1)
{
	QRegExp rx(scene1 + " -> wait for [0-9]* seconds -> .*");
	int count = ui->sceneRoundTrips->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->sceneRoundTrips->item(i);
		QString itemString =
			item->data(Qt::UserRole).toString();

		if (rx.exactMatch(itemString)) {
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

	QListWidgetItem *item = ui->switches->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->switches) {
		if (window.compare(s.window.c_str()) == 0) {
			string name = GetWeakSourceName(s.scene);
			ui->scenes->setCurrentText(name.c_str());
			ui->windows->setCurrentText(window);
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

	QListWidgetItem *item = ui->screenRegions->item(idx);

	QString region = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->screenRegionSwitches) {
		if (region.compare(s.regionStr.c_str()) == 0) {
			string name = GetWeakSourceName(s.scene);
			ui->screenRegionScenes->setCurrentText(name.c_str());
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

	QListWidgetItem *item = ui->pauseScenes->item(idx);

	QString scene = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->pauseScenesSwitches) {
		string name = GetWeakSourceName(s);
		if (scene.compare(name.c_str()) == 0) {
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

	QListWidgetItem *item = ui->pauseWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->pauseWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
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

	QListWidgetItem *item = ui->ignoreWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->ignoreWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
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

	QListWidgetItem *item = ui->sceneRoundTrips->item(idx);

	QString sceneRoundTrip = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->sceneRoundTripSwitches) {
		if (sceneRoundTrip.compare(s.sceneRoundTripStr.c_str()) == 0) {
			string scene1 = GetWeakSourceName(s.scene1);
			string scene2 = GetWeakSourceName(s.scene2);
			int delay = s.delay;
			ui->sceneRoundTripScenes1->setCurrentText(scene1.c_str());
			ui->sceneRoundTripScenes2->setCurrentText(scene2.c_str());
			ui->sceneRoundTripSpinBox->setValue(delay);
			break;
		}
	}
}

void SceneSwitcher::on_close_clicked()
{
	done(0);
}

void SceneSwitcher::on_add_clicked()
{
	QString sceneName = ui->scenes->currentText();
	QString windowName = ui->windows->currentText();
	bool fullscreen = ui->fullscreenCheckBox->isChecked();

	if (windowName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	QVariant v = QVariant::fromValue(windowName);

	QString text = MakeSwitchName(sceneName, windowName, fullscreen);

	int idx = FindByData(windowName);

	if (idx == -1) {
		lock_guard<mutex> lock(switcher->m);
		switcher->switches.emplace_back(source,
			windowName.toUtf8().constData(), fullscreen);

		QListWidgetItem *item = new QListWidgetItem(text,
			ui->switches);
		item->setData(Qt::UserRole, v);
	}
	else {
		QListWidgetItem *item = ui->switches->item(idx);
		item->setText(text);

		string window = windowName.toUtf8().constData();

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->switches) {
				if (s.window == window) {
					s.scene = source;
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
	QListWidgetItem *item = ui->switches->currentItem();
	if (!item)
		return;

	string window =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->switches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.window == window) {
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
	if (sceneName.isEmpty())
		return;
	int minX = ui->screenRegionMinX->value();
	int minY = ui->screenRegionMinY->value();
	int maxX = ui->screenRegionMaxX->value();
	int maxY = ui->screenRegionMaxY->value();

	string regionStr = to_string(minX) + ", " + to_string(minY) + " x " + to_string(maxX) + ", " + to_string(maxY);
	QString region = QString::fromStdString(regionStr);

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	QVariant v = QVariant::fromValue(region);

	QString text = MakeScreenRegionSwitchName(sceneName, minX, minY, maxX, maxY);

	int idx = ScreenRegionFindByData(region);

	if (idx == -1) {
		QListWidgetItem *item = new QListWidgetItem(text,
			ui->screenRegions);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->screenRegionSwitches.emplace_back(source,
			minX, minY, maxX, maxY, regionStr);
	}
	else {
		QListWidgetItem *item = ui->screenRegions->item(idx);
		item->setText(text);

		string curRegion = region.toUtf8().constData();

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->screenRegionSwitches) {
				if (s.regionStr == curRegion) {
					s.scene = source;
					break;
				}
			}
		}

		ui->screenRegions->sortItems();
	}
}

void SceneSwitcher::on_screenRegionRemove_clicked()
{
	QListWidgetItem *item = ui->screenRegions->currentItem();
	if (!item)
		return;

	string region =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->screenRegionSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.regionStr == region) {
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

	QList<QListWidgetItem *> items = ui->pauseScenes->findItems(sceneName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item = new QListWidgetItem(sceneName,
			ui->pauseScenes);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->pauseScenesSwitches.emplace_back(source);
		ui->pauseScenes->sortItems();
	}
}

void SceneSwitcher::on_pauseScenesRemove_clicked()
{
	QListWidgetItem *item = ui->pauseScenes->currentItem();
	if (!item)
		return;

	QString pauseScene =
		item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->pauseScenesSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s == GetWeakSourceByQString(pauseScene)) {
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

	QList<QListWidgetItem *> items = ui->pauseWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item = new QListWidgetItem(windowName,
			ui->pauseWindows);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->pauseWindowsSwitches.emplace_back(windowName.toUtf8().constData());
		ui->pauseWindows->sortItems();
	}
}

void SceneSwitcher::on_pauseWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->pauseWindows->currentItem();
	if (!item)
		return;

	QString windowName =
		item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->pauseWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
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

	QList<QListWidgetItem *> items = ui->ignoreWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item = new QListWidgetItem(windowName,
			ui->ignoreWindows);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->ignoreWindowsSwitches.emplace_back(windowName.toUtf8().constData());
		ui->ignoreWindows->sortItems();
	}
}

void SceneSwitcher::on_ignoreWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->ignoreWindows->currentItem();
	if (!item)
		return;

	QString windowName =
		item->data(Qt::UserRole).toString();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->ignoreWindowsSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
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
	if (scene1Name.isEmpty() || scene2Name.isEmpty())
		return;
	int delay = ui->sceneRoundTripSpinBox->value();

	if (scene1Name == scene2Name)
		return;

	OBSWeakSource source1 = GetWeakSourceByQString(scene1Name);
	OBSWeakSource source2 = GetWeakSourceByQString(scene2Name);

	QString text = MakeSceneRoundTripSwitchName(scene1Name, scene2Name, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = SceneRoundTripFindByData(scene1Name);

	if (idx == -1) {
		QListWidgetItem *item = new QListWidgetItem(text,
			ui->sceneRoundTrips);
		item->setData(Qt::UserRole, v);

		lock_guard<mutex> lock(switcher->m);
		switcher->sceneRoundTripSwitches.emplace_back(source1, source2, delay, text.toUtf8().constData());
	}
	else {
		QListWidgetItem *item = ui->sceneRoundTrips->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->sceneRoundTripSwitches) {
				if (s.scene1 == source1 && s.scene2 == source2) {
					s.delay = delay;
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
	QListWidgetItem *item = ui->sceneRoundTrips->currentItem();
	if (!item)
		return;

	string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->sceneRoundTripSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.sceneRoundTripStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SceneSwitcher::on_startAtLaunch_toggled(bool value)
{
	if (loading)
		return;

	lock_guard<mutex> lock(switcher->m);
	switcher->startAtLaunch = value;
}

void SceneSwitcher::UpdateNonMatchingScene(const QString &name)
{
	obs_source_t *scene = obs_get_source_by_name(
		name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

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

void SceneSwitcher::on_noMatchSwitchScene_currentTextChanged(
	const QString &text)
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
	if (switcher->th.joinable()) {
		switcher->Stop();
		SetStopped();
	}
	else {
		if (switcher->stop){
			switcher->Start();
			SetStarted();
		}
	}
}

static void SaveSceneSwitcher(obs_data_t *save_data, bool saving, void *)
{
	if (saving) {
		lock_guard<mutex> lock(switcher->m);
		obs_data_t *obj = obs_data_create();
		obs_data_array_t *array = obs_data_array_create();
		obs_data_array_t *screenRegionArray = obs_data_array_create();
		obs_data_array_t *pauseScenesArray = obs_data_array_create();
		obs_data_array_t *pauseWindowsArray = obs_data_array_create();
		obs_data_array_t *ignoreWindowsArray = obs_data_array_create();
		obs_data_array_t *sceneRoundTripArray = obs_data_array_create();

		switcher->Prune();

		for (SceneSwitch &s : switcher->switches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source = obs_weak_source_get_source(
				s.scene);
			if (source) {
				const char *n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "scene", n);
				obs_data_set_string(array_obj, "window_title",
					s.window.c_str());
				obs_data_set_bool(array_obj, "fullscreen",
					s.fullscreen);
				obs_data_array_push_back(array, array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		for (ScreenRegionSwitch &s : switcher->screenRegionSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source = obs_weak_source_get_source(
				s.scene);
			if (source) {
				const char *n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "screenRegionScene", n);
				obs_data_set_int(array_obj, "minX",
					s.minX);
				obs_data_set_int(array_obj, "minY",
					s.minY);
				obs_data_set_int(array_obj, "maxX",
					s.maxX);
				obs_data_set_int(array_obj, "maxY",
					s.maxY);
				obs_data_set_string(array_obj, "screenRegionStr",
					s.regionStr.c_str());
				obs_data_array_push_back(screenRegionArray, array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		for (OBSWeakSource &scene : switcher->pauseScenesSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source = obs_weak_source_get_source(
				scene);
			if (source) {
				const char *n = obs_source_get_name(source);
				obs_data_set_string(array_obj, "pauseScene", n);
				obs_data_array_push_back(pauseScenesArray, array_obj);
				obs_source_release(source);
			}

			obs_data_release(array_obj);
		}

		for (string &window : switcher->pauseWindowsSwitches) {
			obs_data_t *array_obj = obs_data_create();
			obs_data_set_string(array_obj, "pauseWindow", window.c_str());
			obs_data_array_push_back(pauseWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (string &window : switcher->ignoreWindowsSwitches) {
			obs_data_t *array_obj = obs_data_create();
			obs_data_set_string(array_obj, "ignoreWindow", window.c_str());
			obs_data_array_push_back(ignoreWindowsArray, array_obj);
			obs_data_release(array_obj);
		}

		for (SceneRoundTripSwitch &s : switcher->sceneRoundTripSwitches) {
			obs_data_t *array_obj = obs_data_create();

			obs_source_t *source1 = obs_weak_source_get_source(
				s.scene1);
			obs_source_t *source2 = obs_weak_source_get_source(
				s.scene2);
			if (source1 && s.scene2) {
				const char *n1 = obs_source_get_name(source1);
				const char *n2 = obs_source_get_name(source2);
				obs_data_set_string(array_obj, "sceneRoundTripScene1", n1);
				obs_data_set_string(array_obj, "sceneRoundTripScene2", n2);
				obs_data_set_int(array_obj, "sceneRoundTripDelay",
					s.delay);
				obs_data_set_string(array_obj, "sceneRoundTripStr", s.sceneRoundTripStr.c_str());
				obs_data_array_push_back(sceneRoundTripArray, array_obj);
				obs_source_release(source1);
				obs_source_release(source2);
			}

			obs_data_release(array_obj);
		}

		string nonMatchingSceneName =
			GetWeakSourceName(switcher->nonMatchingScene);

		obs_data_set_int(obj, "interval", switcher->interval);
		obs_data_set_string(obj, "non_matching_scene",
			nonMatchingSceneName.c_str());
		obs_data_set_bool(obj, "switch_if_not_matching",
			switcher->switchIfNotMatching);
		obs_data_set_bool(obj, "active", switcher->th.joinable());

		obs_data_set_array(obj, "switches", array);
		obs_data_set_array(obj, "screenRegion", screenRegionArray);
		obs_data_set_array(obj, "pauseScenes", pauseScenesArray);
		obs_data_set_array(obj, "pauseWindows", pauseWindowsArray);
		obs_data_set_array(obj, "ignoreWindows", ignoreWindowsArray);
		obs_data_set_array(obj, "sceneRoundTrip", sceneRoundTripArray);

		obs_data_set_obj(save_data, "advanced-scene-switcher", obj);

		obs_data_array_release(array);
		obs_data_array_release(screenRegionArray);
		obs_data_array_release(pauseScenesArray);
		obs_data_array_release(pauseWindowsArray);
		obs_data_array_release(ignoreWindowsArray);
		obs_data_array_release(sceneRoundTripArray);
		obs_data_release(obj);
	}
	else {
		switcher->m.lock();

		obs_data_t *obj = obs_data_get_obj(save_data,
			"advanced-scene-switcher");
		obs_data_array_t *array = obs_data_get_array(obj, "switches");
		size_t count = obs_data_array_count(array);

		if (!obj)
			obj = obs_data_create();

		obs_data_set_default_int(obj, "interval", DEFAULT_INTERVAL);

		switcher->interval = obs_data_get_int(obj, "interval");
		switcher->switchIfNotMatching =
			obs_data_get_bool(obj, "switch_if_not_matching");
		string nonMatchingScene =
			obs_data_get_string(obj, "non_matching_scene");
		bool active = obs_data_get_bool(obj, "active");

		switcher->nonMatchingScene =
			GetWeakSourceByName(nonMatchingScene.c_str());

		switcher->switches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(array, i);

			const char *scene =
				obs_data_get_string(array_obj, "scene");
			const char *window =
				obs_data_get_string(array_obj, "window_title");
			bool fullscreen =
				obs_data_get_bool(array_obj, "fullscreen");

			switcher->switches.emplace_back(
				GetWeakSourceByName(scene),
				window, fullscreen);

			obs_data_release(array_obj);
		}

		obs_data_array_release(array);

		obs_data_array_t *screenRegionArray = obs_data_get_array(obj, "screenRegion");
		count = obs_data_array_count(screenRegionArray);

		switcher->screenRegionSwitches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(screenRegionArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "screenRegionScene");
			int minX = obs_data_get_int(array_obj, "minX");
			int minY = obs_data_get_int(array_obj, "minY");
			int maxX = obs_data_get_int(array_obj, "maxX");
			int maxY = obs_data_get_int(array_obj, "maxY");
			string regionStr =
				obs_data_get_string(array_obj, "screenRegionStr");

			switcher->screenRegionSwitches.emplace_back(
				GetWeakSourceByName(scene),
				minX, minY, maxX, maxY, regionStr);

			obs_data_release(array_obj);
		}

		obs_data_array_release(screenRegionArray);

		obs_data_array_t *pauseScenesArray = obs_data_get_array(obj, "pauseScenes");
		count = obs_data_array_count(pauseScenesArray);

		switcher->pauseScenesSwitches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(pauseScenesArray, i);

			const char *scene =
				obs_data_get_string(array_obj, "pauseScene");

			switcher->pauseScenesSwitches.emplace_back(GetWeakSourceByName(scene));

			obs_data_release(array_obj);
		}

		obs_data_array_release(pauseScenesArray);

		obs_data_array_t *pauseWindowsArray = obs_data_get_array(obj, "pauseWindows");
		count = obs_data_array_count(pauseWindowsArray);

		switcher->pauseWindowsSwitches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(pauseWindowsArray, i);

			const char *window =
				obs_data_get_string(array_obj, "pauseWindow");

			switcher->pauseWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		obs_data_array_release(pauseWindowsArray);

		obs_data_array_t *ignoreWindowsArray = obs_data_get_array(obj, "ignoreWindows");
		count = obs_data_array_count(ignoreWindowsArray);

		switcher->ignoreWindowsSwitches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(ignoreWindowsArray, i);

			const char *window =
				obs_data_get_string(array_obj, "ignoreWindow");

			switcher->ignoreWindowsSwitches.emplace_back(window);

			obs_data_release(array_obj);
		}

		obs_data_array_release(ignoreWindowsArray);

		obs_data_array_t *sceneRoundTripArray = obs_data_get_array(obj, "sceneRoundTrip");
		count = obs_data_array_count(sceneRoundTripArray);

		switcher->sceneRoundTripSwitches.clear();

		for (size_t i = 0; i < count; i++) {
			obs_data_t *array_obj = obs_data_array_item(sceneRoundTripArray, i);

			const char *scene1 =
				obs_data_get_string(array_obj, "sceneRoundTripScene1");
			const char *scene2 =
				obs_data_get_string(array_obj, "sceneRoundTripScene2");
			int delay = obs_data_get_int(array_obj, "sceneRoundTripDelay");
			const char *sceneRoundTripStr =
				obs_data_get_string(array_obj, "sceneRoundTripStr");

			switcher->sceneRoundTripSwitches.emplace_back(
				GetWeakSourceByName(scene1),
				GetWeakSourceByName(scene2),
				delay,
				sceneRoundTripStr);

			obs_data_release(array_obj);
		}

		obs_data_array_release(sceneRoundTripArray);

		obs_data_release(obj);

		switcher->m.unlock();

		if (active && !switcher->stop)
			switcher->Start();
		else
			switcher->Stop();
	}
}

void SwitcherData::Thread()
{
	chrono::duration<long long, milli> duration =
		chrono::milliseconds(interval);
	string lastTitle;
	string title;

	while (true) {
		unique_lock<mutex> lock(waitMutex);
		OBSWeakSource scene;
		bool match = false;
		bool fullscreen = false;
		bool pause = false;
		bool sceneRoundTripActive = false;

		cv.wait_for(lock, duration);
		threadEndMutex.lock();
		if (stop) {
		//if (test.value()){
			threadEndMutex.unlock();
			break;
		}
		else threadEndMutex.unlock();

		obs_source_t *currentSource =
			obs_frontend_get_current_scene();

		for (OBSWeakSource &s : pauseScenesSwitches) {
			OBSWeakSource ws = obs_source_get_weak_source(currentSource);
			if (s == ws) {
				pause = true;
				obs_weak_source_release(ws);
				break;
			}
			obs_weak_source_release(ws);
		}

		if (!pause){
			GetCurrentWindowTitle(title);
			for (string &window : pauseWindowsSwitches) {
				if (window == title) {
					pause = true;
					break;
				}
			}
		}

		if (pause)
			continue;

		for (SceneRoundTripSwitch &s : sceneRoundTripSwitches) {
			OBSWeakSource ws = obs_source_get_weak_source(currentSource);
			if (s.scene1 == ws) {
				sceneRoundTripActive = true;
				int dur = s.delay * 1000 - interval;
				if (dur > 30)
					cv.wait_for(lock, chrono::milliseconds(dur));
				else
					cv.wait_for(lock, chrono::milliseconds(30));
				obs_source_t *source =
					obs_weak_source_get_source(s.scene2);
				obs_source_t *currentSource2 =
					obs_frontend_get_current_scene();

				if (currentSource == currentSource2){
					obs_frontend_set_current_scene(source);
					obs_source_release(source);
					obs_source_release(currentSource2);
					obs_weak_source_release(ws);
					break;
				}
				obs_source_release(currentSource2);
			}
			obs_weak_source_release(ws);
		}
		obs_source_release(currentSource);

		if (sceneRoundTripActive)
			continue;

		duration = chrono::milliseconds(interval);

		GetCurrentWindowTitle(title);

		for (auto &window : ignoreWindowsSwitches){
			if (window == title){
				title = lastTitle;
				break;
			}
		}
		lastTitle = title;

		switcher->Prune();

		for (SceneSwitch &s : switches) {
			if (s.window == title) {
				match = true;
				scene = s.scene;
				fullscreen = s.fullscreen;
				break;
			}
		}

		/* try regex */
		if (!match) {
			for (SceneSwitch &s : switches) {
				try {

					bool matches = regex_match(
						title, regex(s.window));
					if (matches) {
						match = true;
						scene = s.scene;
						fullscreen = s.fullscreen;
						break;
					}
				}
				catch (const regex_error &) {}
			}
		}

		if (!match){
			pair<int, int> cursorPos = getCursorPos();
			int minRegionSize = 99999;

			for (auto &s : screenRegionSwitches){
				if (cursorPos.first >= s.minX && cursorPos.second >= s.minY && cursorPos.first <= s.maxX && cursorPos.second <= s.maxY)
				{
					int regionSize = (s.maxX - s.minX) + (s.maxY - s.minY);
					if (regionSize < minRegionSize)
					{
						match = true;
						scene = s.scene;
						minRegionSize = regionSize;
					}
				}
			}
		}

		match = match && (!fullscreen || (fullscreen && isFullscreen()));

		if (!match && switchIfNotMatching &&
			nonMatchingScene) {
			match = true;
			scene = nonMatchingScene;
		}

		if (match) {
			obs_source_t *source =
				obs_weak_source_get_source(scene);
			obs_source_t *currentSource =
				obs_frontend_get_current_scene();

			if (source && source != currentSource)
				obs_frontend_set_current_scene(source);

			obs_source_release(currentSource);
			obs_source_release(source);
		}
	}
}

void SwitcherData::Start()
{
	if (!th.joinable()){
		threadEndMutex.lock();
		//test.setValue(false);
		stop = false;
		threadEndMutex.unlock();
		switcher->th = thread([]() {switcher->Thread(); });
	}
}

void SwitcherData::Stop()
{
	threadEndMutex.lock();
	stop = true;
	//test.setValue(true);
	cv.notify_one();
	threadEndMutex.unlock();
	if (th.joinable())
		th.join();
}

//HOTKEY

void startStopHotkeyFunc(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(hotkey);
	if (pressed)
	{
		if (switcher->th.joinable())
			switcher->Stop();
		else
			switcher->Start();
	}

	obs_data_array *hotkeyData = obs_hotkey_save(id);
	if (hotkeyData != NULL) {
		char *path = obs_module_config_path("");
		boost::filesystem::create_directories(path);
		ofstream file;
		file.open(string(path).append("hotkey.txt"), ofstream::trunc);
		if (file.is_open()) {
			size_t num = obs_data_array_count(hotkeyData);
			for (size_t i = 0; i < num; i++) {
				obs_data_t *data = obs_data_array_item(hotkeyData, i);
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

string loadConfigFile(string filename) {
	ifstream settingsFile;
	char *path = obs_module_config_path("");
	settingsFile.open(string(path).append(filename));
	string value;
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

void loadKeybinding(obs_hotkey_id hotkeyId) {
	string temp = loadConfigFile("hotkey.txt");
	if (!temp.empty())
	{
		obs_data_array_t *hotkeyData = obs_data_array_create();
		obs_data_t *data = obs_data_create_from_json(temp.c_str());
		obs_data_array_insert(hotkeyData, 0, data);
		obs_data_release(data);
		obs_hotkey_load(hotkeyId, hotkeyData);
		obs_data_array_release(hotkeyData);
	}
}

extern "C" void FreeSceneSwitcher()
{
	delete switcher;
	switcher = nullptr;
}

static void OBSEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT){
		FreeSceneSwitcher();
	}
}

extern "C" void InitSceneSwitcher()
{
	QAction *action = (QAction*)obs_frontend_add_tools_menu_qaction(
		obs_module_text("Advanced Scene Switcher"));


	switcher = new SwitcherData;

	auto cb = []()
	{
		obs_frontend_push_ui_translation(obs_module_get_string);

		QMainWindow *window =
			(QMainWindow*)obs_frontend_get_main_window();

		SceneSwitcher ss(window);
		ss.exec();

		obs_frontend_pop_ui_translation();
	};

	obs_frontend_add_save_callback(SaveSceneSwitcher, nullptr);
	obs_frontend_add_event_callback(OBSEvent, nullptr);

	action->connect(action, &QAction::triggered, cb);

	obs_hotkey_id pauseHotkeyId = obs_hotkey_register_frontend("startStopSwitcherHotkey", "Toggle Start/Stop for the Advanced Scene Switcher", startStopHotkeyFunc, NULL);
	loadKeybinding(pauseHotkeyId);
}