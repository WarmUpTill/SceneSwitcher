#include <regex>

#include "headers/advanced-scene-switcher.hpp"

void AdvSceneSwitcher::on_pauseScenesAdd_clicked()
{
	QString sceneName = ui->pauseScenesScenes->currentText();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	QVariant v = QVariant::fromValue(sceneName);

	QList<QListWidgetItem *> items =
		ui->pauseScenes->findItems(sceneName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(sceneName, ui->pauseScenes);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->pauseScenesSwitches.emplace_back(source);
		ui->pauseScenes->sortItems();
	}
}

void AdvSceneSwitcher::on_pauseScenesRemove_clicked()
{
	QListWidgetItem *item = ui->pauseScenes->currentItem();
	if (!item)
		return;

	QString pauseScene = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
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

void AdvSceneSwitcher::on_pauseWindowsAdd_clicked()
{
	QString windowName = ui->pauseWindowsWindows->currentText();

	if (windowName.isEmpty())
		return;

	QVariant v = QVariant::fromValue(windowName);

	QList<QListWidgetItem *> items =
		ui->pauseWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->pauseWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->pauseWindowsSwitches.emplace_back(
			windowName.toUtf8().constData());
		ui->pauseWindows->sortItems();
	}
}

void AdvSceneSwitcher::on_pauseWindowsRemove_clicked()
{
	QListWidgetItem *item = ui->pauseWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
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

void AdvSceneSwitcher::on_pauseScenes_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->pauseScenes->item(idx);

	QString scene = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->pauseScenesSwitches) {
		std::string name = GetWeakSourceName(s);
		if (scene.compare(name.c_str()) == 0) {
			ui->pauseScenesScenes->setCurrentText(name.c_str());
			break;
		}
	}
}

void AdvSceneSwitcher::on_pauseWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->pauseWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->pauseWindowsSwitches) {
		if (window.compare(s.c_str()) == 0) {
			ui->pauseWindowsWindows->setCurrentText(s.c_str());
			break;
		}
	}
}

int AdvSceneSwitcher::PauseScenesFindByData(const QString &scene)
{
	int count = ui->pauseScenes->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->pauseScenes->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == scene) {
			idx = i;
			break;
		}
	}

	return idx;
}

int AdvSceneSwitcher::PauseWindowsFindByData(const QString &window)
{
	int count = ui->pauseWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->pauseWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

bool SwitcherData::checkPause()
{
	bool pause = false;
	obs_source_t *currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t *ws = obs_source_get_weak_source(currentSource);

	for (OBSWeakSource &s : pauseScenesSwitches) {
		if (s == ws) {
			pause = true;
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);

	std::string title;
	if (!pause) {
		//lock.unlock();
		GetCurrentWindowTitle(title);
		//lock.lock();
		for (std::string &window : pauseWindowsSwitches) {
			if (window == title) {
				pause = true;
				break;
			}
		}
	}

	if (!pause) {
		//lock.unlock();
		GetCurrentWindowTitle(title);
		//lock.lock();
		for (std::string &window : pauseWindowsSwitches) {
			try {
				bool matches = std::regex_match(
					title, std::regex(window));
				if (matches) {
					pause = true;
					break;
				}
			} catch (const std::regex_error &) {
			}
		}
	}

	if (verbose && pause)
		blog(LOG_INFO, "pause match");

	return pause;
}

void SwitcherData::savePauseSwitches(obs_data_t *obj)
{
	obs_data_array_t *pauseScenesArray = obs_data_array_create();
	for (OBSWeakSource &scene : switcher->pauseScenesSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(scene);
		if (source) {
			const char *n = obs_source_get_name(source);
			obs_data_set_string(array_obj, "pauseScene", n);
			obs_data_array_push_back(pauseScenesArray, array_obj);
			obs_source_release(source);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "pauseScenes", pauseScenesArray);
	obs_data_array_release(pauseScenesArray);

	obs_data_array_t *pauseWindowsArray = obs_data_array_create();
	for (std::string &window : switcher->pauseWindowsSwitches) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "pauseWindow", window.c_str());
		obs_data_array_push_back(pauseWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "pauseWindows", pauseWindowsArray);
	obs_data_array_release(pauseWindowsArray);
}

void SwitcherData::loadPauseSwitches(obs_data_t *obj)
{
	switcher->pauseScenesSwitches.clear();

	obs_data_array_t *pauseScenesArray =
		obs_data_get_array(obj, "pauseScenes");
	size_t count = obs_data_array_count(pauseScenesArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseScenesArray, i);

		const char *scene =
			obs_data_get_string(array_obj, "pauseScene");

		switcher->pauseScenesSwitches.emplace_back(
			GetWeakSourceByName(scene));

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseScenesArray);

	switcher->pauseWindowsSwitches.clear();

	obs_data_array_t *pauseWindowsArray =
		obs_data_get_array(obj, "pauseWindows");
	count = obs_data_array_count(pauseWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(pauseWindowsArray, i);

		const char *window =
			obs_data_get_string(array_obj, "pauseWindow");

		switcher->pauseWindowsSwitches.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(pauseWindowsArray);
}

void AdvSceneSwitcher::setupPauseTab()
{
	populateSceneSelection(ui->pauseScenesScenes, false);
	populateWindowSelection(ui->pauseWindowsWindows);

	for (auto &scene : switcher->pauseScenesSwitches) {
		std::string sceneName = GetWeakSourceName(scene);
		QString text = QString::fromStdString(sceneName);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseScenes);
		item->setData(Qt::UserRole, text);
	}

	for (auto &window : switcher->pauseWindowsSwitches) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->pauseWindows);
		item->setData(Qt::UserRole, text);
	}
}
