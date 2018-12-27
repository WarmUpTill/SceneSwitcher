#include "headers/advanced-scene-switcher.hpp"

void SwitcherData::checkScreenRegionSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition)
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

void SceneSwitcher::updateScreenRegionCursorPos()
{
	pair<int, int> position = getCursorPos();
	ui->cursorXPosition->setText(QString::number(position.first));
	;
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
