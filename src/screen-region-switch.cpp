#include <QTimer>

#include "headers/advanced-scene-switcher.hpp"

void SwitcherData::checkScreenRegionSwitch(bool &match, OBSWeakSource &scene,
					   OBSWeakSource &transition)
{
	std::pair<int, int> cursorPos = getCursorPos();
	int minRegionSize = 99999;

	for (auto &s : screenRegionSwitches) {
		if (cursorPos.first >= s.minX && cursorPos.second >= s.minY &&
		    cursorPos.first <= s.maxX && cursorPos.second <= s.maxY) {
			int regionSize = (s.maxX - s.minX) + (s.maxY - s.minY);
			if (regionSize < minRegionSize) {
				match = true;
				scene = s.scene;
				transition = s.transition;
				minRegionSize = regionSize;

				if (verbose)
					blog(LOG_INFO,
					     "Advanced Scene Switcher region match");

				break;
			}
		}
	}
}

void SceneSwitcher::updateScreenRegionCursorPos()
{
	std::pair<int, int> position = getCursorPos();
	ui->cursorXPosition->setText(QString::number(position.first));
	ui->cursorYPosition->setText(QString::number(position.second));
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

	std::string regionStr =
		std::to_string(minX) + ", " + std::to_string(minY) + " x " +
		std::to_string(maxX) + ", " + std::to_string(maxY);
	QString region = QString::fromStdString(regionStr);

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);
	QVariant v = QVariant::fromValue(region);

	QString text = MakeScreenRegionSwitchName(sceneName, transitionName,
						  minX, minY, maxX, maxY);

	int idx = ScreenRegionFindByData(region);

	if (idx == -1) {
		QListWidgetItem *item =
			new QListWidgetItem(text, ui->screenRegions);
		item->setData(Qt::UserRole, v);

		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->screenRegionSwitches.emplace_back(
			source, transition, minX, minY, maxX, maxY, regionStr);
	} else {
		QListWidgetItem *item = ui->screenRegions->item(idx);
		item->setText(text);

		std::string curRegion = region.toUtf8().constData();

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->screenRegionSwitches) {
				if (s.regionStr == curRegion) {
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
	QListWidgetItem *item = ui->screenRegions->currentItem();
	if (!item)
		return;

	std::string region =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
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

void SceneSwitcher::on_screenRegions_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->screenRegions->item(idx);

	QString region = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->screenRegionSwitches) {
		if (region.compare(s.regionStr.c_str()) == 0) {
			std::string name = GetWeakSourceName(s.scene);
			std::string transitionName =
				GetWeakSourceName(s.transition);
			ui->screenRegionScenes->setCurrentText(name.c_str());
			ui->screenRegionsTransitions->setCurrentText(
				transitionName.c_str());
			ui->screenRegionMinX->setValue(s.minX);
			ui->screenRegionMinY->setValue(s.minY);
			ui->screenRegionMaxX->setValue(s.maxX);
			ui->screenRegionMaxY->setValue(s.maxY);
			break;
		}
	}
}

int SceneSwitcher::ScreenRegionFindByData(const QString &region)
{
	int count = ui->screenRegions->count();
	int idx = -1;

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->screenRegions->item(i);
		QString itemRegion = item->data(Qt::UserRole).toString();

		if (itemRegion == region) {
			idx = i;
			break;
		}
	}

	return idx;
}

void SceneSwitcher::on_screenRegionUp_clicked()
{
	int index = ui->screenRegions->currentRow();
	if (index != -1 && index != 0) {
		ui->screenRegions->insertItem(
			index - 1, ui->screenRegions->takeItem(index));
		ui->screenRegions->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->screenRegionSwitches.begin() + index,
			  switcher->screenRegionSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_screenRegionDown_clicked()
{
	int index = ui->screenRegions->currentRow();
	if (index != -1 && index != ui->screenRegions->count() - 1) {
		ui->screenRegions->insertItem(
			index + 1, ui->screenRegions->takeItem(index));
		ui->screenRegions->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->screenRegionSwitches.begin() + index,
			  switcher->screenRegionSwitches.begin() + index + 1);
	}
}

void SwitcherData::saveScreenRegionSwitches(obs_data_t *obj)
{
	obs_data_array_t *screenRegionArray = obs_data_array_create();
	for (ScreenRegionSwitch &s : switcher->screenRegionSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if (source && transition) {
			const char *sceneName = obs_source_get_name(source);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "screenRegionScene",
					    sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_int(array_obj, "minX", s.minX);
			obs_data_set_int(array_obj, "minY", s.minY);
			obs_data_set_int(array_obj, "maxX", s.maxX);
			obs_data_set_int(array_obj, "maxY", s.maxY);
			obs_data_set_string(array_obj, "screenRegionStr",
					    s.regionStr.c_str());
			obs_data_array_push_back(screenRegionArray, array_obj);
			obs_source_release(source);
			obs_source_release(transition);
		}

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "screenRegion", screenRegionArray);
	obs_data_array_release(screenRegionArray);
}

void SwitcherData::loadScreenRegionSwitches(obs_data_t *obj)
{
	switcher->screenRegionSwitches.clear();

	obs_data_array_t *screenRegionArray =
		obs_data_get_array(obj, "screenRegion");
	size_t count = obs_data_array_count(screenRegionArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj =
			obs_data_array_item(screenRegionArray, i);

		const char *scene =
			obs_data_get_string(array_obj, "screenRegionScene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		int minX = obs_data_get_int(array_obj, "minX");
		int minY = obs_data_get_int(array_obj, "minY");
		int maxX = obs_data_get_int(array_obj, "maxX");
		int maxY = obs_data_get_int(array_obj, "maxY");
		std::string regionStr =
			obs_data_get_string(array_obj, "screenRegionStr");

		switcher->screenRegionSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), minX, minY, maxX,
			maxY, regionStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(screenRegionArray);
}

void SceneSwitcher::setupRegionTab()
{
	populateSceneSelection(ui->screenRegionScenes, false);
	populateTransitionSelection(ui->screenRegionsTransitions);

	for (auto &s : switcher->screenRegionSwitches) {
		std::string sceneName = GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString text = MakeScreenRegionSwitchName(
			sceneName.c_str(), transitionName.c_str(), s.minX,
			s.minY, s.maxX, s.maxY);

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->screenRegions);
		item->setData(Qt::UserRole, s.regionStr.c_str());
	}

	// screen region cursor position
	QTimer *screenRegionTimer = new QTimer(this);
	connect(screenRegionTimer, SIGNAL(timeout()), this,
		SLOT(updateScreenRegionCursorPos()));
	screenRegionTimer->start(1000);
}
