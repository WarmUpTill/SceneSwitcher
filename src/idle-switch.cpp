#include "headers/advanced-scene-switcher.hpp"

void SwitcherData::checkIdleSwitch(bool &match, OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (!idleData.idleEnable)
		return;

	std::string title;
	bool ignoreIdle = false;
	//lock.unlock();
	GetCurrentWindowTitle(title);
	//lock.lock();

	for (std::string &window : ignoreIdleWindows) {
		if (window == title) {
			ignoreIdle = true;
			break;
		}
	}

	if (!ignoreIdle) {
		for (std::string &window : ignoreIdleWindows) {
			try {
				bool matches = std::regex_match(
					title, std::regex(window));
				if (matches) {
					ignoreIdle = true;
					break;
				}
			} catch (const std::regex_error &) {
			}
		}
	}

	if (!ignoreIdle && secondsSinceLastInput() > idleData.time) {
		if (idleData.alreadySwitched)
			return;
		scene = (idleData.usePreviousScene) ? previousScene
						    : idleData.scene;
		transition = idleData.transition;
		match = true;
		idleData.alreadySwitched = true;

		if (verbose)
			blog(LOG_INFO, "Advanced Scene Switcher idle match");
	} else
		idleData.alreadySwitched = false;
}

void SceneSwitcher::on_idleCheckBox_stateChanged(int state)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	if (!state) {
		ui->idleScenes->setDisabled(true);
		ui->idleSpinBox->setDisabled(true);
		ui->idleTransitions->setDisabled(true);

		switcher->idleData.idleEnable = false;
	} else {
		ui->idleScenes->setDisabled(false);
		ui->idleSpinBox->setDisabled(false);
		ui->idleTransitions->setDisabled(false);

		switcher->idleData.idleEnable = true;

		UpdateIdleDataTransition(ui->idleTransitions->currentText());
		UpdateIdleDataScene(ui->idleScenes->currentText());
	}
}

void SceneSwitcher::UpdateIdleDataTransition(const QString &name)
{
	obs_weak_source_t *transition = GetWeakTransitionByQString(name);
	switcher->idleData.transition = transition;
}

void SceneSwitcher::UpdateIdleDataScene(const QString &name)
{
	switcher->idleData.usePreviousScene = (name == PREVIOUS_SCENE_NAME);
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->idleData.scene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void SceneSwitcher::on_idleTransitions_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateIdleDataTransition(text);
}

void SceneSwitcher::on_idleScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateIdleDataScene(text);
}

void SceneSwitcher::on_idleSpinBox_valueChanged(int i)
{
	if (loading)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->idleData.time = i;
}

void SceneSwitcher::on_ignoreIdleWindows_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->ignoreIdleWindows->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &w : switcher->ignoreIdleWindows) {
		if (window.compare(w.c_str()) == 0) {
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

	QList<QListWidgetItem *> items =
		ui->ignoreIdleWindows->findItems(windowName, Qt::MatchExactly);

	if (items.size() == 0) {
		QListWidgetItem *item =
			new QListWidgetItem(windowName, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->ignoreIdleWindows.emplace_back(
			windowName.toUtf8().constData());
		ui->ignoreIdleWindows->sortItems();
	}
}

void SceneSwitcher::on_ignoreIdleRemove_clicked()
{
	QListWidgetItem *item = ui->ignoreIdleWindows->currentItem();
	if (!item)
		return;

	QString windowName = item->data(Qt::UserRole).toString();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		auto &windows = switcher->ignoreIdleWindows;

		for (auto it = windows.begin(); it != windows.end(); ++it) {
			auto &s = *it;

			if (s == windowName.toUtf8().constData()) {
				windows.erase(it);
				break;
			}
		}
	}

	delete item;
}

int SceneSwitcher::IgnoreIdleWindowsFindByData(const QString &window)
{
	int count = ui->ignoreIdleWindows->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->ignoreIdleWindows->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == window) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SwitcherData::saveIdleSwitches(obs_data_t *obj)
{
	obs_data_array_t *ignoreIdleWindowsArray = obs_data_array_create();
	for (std::string &window : switcher->ignoreIdleWindows) {
		obs_data_t *array_obj = obs_data_create();
		obs_data_set_string(array_obj, "window", window.c_str());
		obs_data_array_push_back(ignoreIdleWindowsArray, array_obj);
		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "ignoreIdleWindows", ignoreIdleWindowsArray);
	obs_data_array_release(ignoreIdleWindowsArray);

	std::string idleSceneName = GetWeakSourceName(switcher->idleData.scene);
	std::string idleTransitionName =
		GetWeakSourceName(switcher->idleData.transition);
	obs_data_set_bool(obj, "idleEnable", switcher->idleData.idleEnable);
	obs_data_set_string(obj, "idleSceneName",
			    switcher->idleData.usePreviousScene
				    ? PREVIOUS_SCENE_NAME
				    : idleSceneName.c_str());
	obs_data_set_string(obj, "idleTransitionName",
			    idleTransitionName.c_str());
	obs_data_set_int(obj, "idleTime", switcher->idleData.time);
}

void SwitcherData::loadIdleSwitches(obs_data_t *obj)
{
	switcher->ignoreIdleWindows.clear();

	obs_data_array_t *ignoreIdleWindowsArray =
		obs_data_get_array(obj, "ignoreIdleWindows");
	size_t count = obs_data_array_count(ignoreIdleWindowsArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(ignoreIdleWindowsArray, i);

		const char *window = obs_data_get_string(array_obj, "window");

		switcher->ignoreIdleWindows.emplace_back(window);

		obs_data_release(array_obj);
	}
	obs_data_array_release(ignoreIdleWindowsArray);

	std::string idleSceneName = obs_data_get_string(obj, "idleSceneName");
	std::string idleTransitionName =
		obs_data_get_string(obj, "idleTransitionName");
	switcher->idleData.scene = GetWeakSourceByName(idleSceneName.c_str());
	switcher->idleData.transition =
		GetWeakTransitionByName(idleTransitionName.c_str());
	obs_data_set_default_bool(obj, "idleEnable", false);
	switcher->idleData.idleEnable = obs_data_get_bool(obj, "idleEnable");
	obs_data_set_default_int(obj, "idleTime", DEFAULT_IDLE_TIME);
	switcher->idleData.time = obs_data_get_int(obj, "idleTime");
	switcher->idleData.usePreviousScene =
		(idleSceneName == PREVIOUS_SCENE_NAME);
}

void SceneSwitcher::setupIdleTab()
{
	populateSceneSelection(ui->idleScenes, true);
	populateTransitionSelection(ui->idleTransitions);
	populateWindowSelection(ui->ignoreIdleWindowsWindows);

	for (auto &window : switcher->ignoreIdleWindows) {
		QString text = QString::fromStdString(window);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->ignoreIdleWindows);
		item->setData(Qt::UserRole, text);
	}

	ui->idleCheckBox->setChecked(switcher->idleData.idleEnable);
	ui->idleScenes->setCurrentText(
		switcher->idleData.usePreviousScene
			? PREVIOUS_SCENE_NAME
			: GetWeakSourceName(switcher->idleData.scene).c_str());
	ui->idleTransitions->setCurrentText(
		GetWeakSourceName(switcher->idleData.transition).c_str());
	ui->idleSpinBox->setValue(switcher->idleData.time);

	if (ui->idleCheckBox->checkState()) {
		ui->idleScenes->setDisabled(false);
		ui->idleSpinBox->setDisabled(false);
		ui->idleTransitions->setDisabled(false);
	} else {
		ui->idleScenes->setDisabled(true);
		ui->idleSpinBox->setDisabled(true);
		ui->idleTransitions->setDisabled(true);
	}
}
