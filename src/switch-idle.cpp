#include <regex>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool IdleData::pause = false;

void SwitcherData::checkIdleSwitch(bool &match, SwitchTarget &target,
				   OBSWeakSource &transition)
{
	if (!idleData.idleEnable || IdleData::pause)
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
		target.type = SwitchTargetType::Scene;
		target.scene = (idleData.usePreviousScene) ? previousScene
							   : idleData.scene;
		transition = idleData.transition;
		match = true;
		idleData.alreadySwitched = true;

		if (verbose)
			idleData.logMatch();
	} else
		idleData.alreadySwitched = false;
}

void AdvSceneSwitcher::on_idleCheckBox_stateChanged(int state)
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

void AdvSceneSwitcher::UpdateIdleDataTransition(const QString &name)
{
	obs_weak_source_t *transition = GetWeakTransitionByQString(name);
	switcher->idleData.transition = transition;
}

void AdvSceneSwitcher::UpdateIdleDataScene(const QString &name)
{
	switcher->idleData.usePreviousScene =
		(name ==
		 obs_module_text("AdvSceneSwitcher.selectPreviousScene"));
	obs_source_t *scene = obs_get_source_by_name(name.toUtf8().constData());
	obs_weak_source_t *ws = obs_source_get_weak_source(scene);

	switcher->idleData.scene = ws;

	obs_weak_source_release(ws);
	obs_source_release(scene);
}

void AdvSceneSwitcher::on_idleTransitions_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateIdleDataTransition(text);
}

void AdvSceneSwitcher::on_idleScenes_currentTextChanged(const QString &text)
{
	if (loading)
		return;

	std::lock_guard<std::mutex> lock(switcher->m);
	UpdateIdleDataScene(text);
}

void AdvSceneSwitcher::on_idleSpinBox_valueChanged(int i)
{
	if (loading)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->idleData.time = i;
}

void AdvSceneSwitcher::on_ignoreIdleWindows_currentRowChanged(int idx)
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

void AdvSceneSwitcher::on_ignoreIdleAdd_clicked()
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

void AdvSceneSwitcher::on_ignoreIdleRemove_clicked()
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

int AdvSceneSwitcher::IgnoreIdleWindowsFindByData(const QString &window)
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
				    ? previous_scene_name
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
	obs_data_set_default_int(obj, "idleTime", default_idle_time);
	switcher->idleData.time = obs_data_get_int(obj, "idleTime");
	switcher->idleData.usePreviousScene =
		(idleSceneName == previous_scene_name);
}

void AdvSceneSwitcher::setupIdleTab()
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
	if (switcher->idleData.usePreviousScene) {
		ui->idleScenes->setCurrentText(obs_module_text(
			"AdvSceneSwitcher.selectPreviousScene"));
	} else {
		ui->idleScenes->setCurrentText(
			GetWeakSourceName(switcher->idleData.scene).c_str());
	}
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
