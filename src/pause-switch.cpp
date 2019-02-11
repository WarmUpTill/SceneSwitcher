#include "headers/advanced-scene-switcher.hpp"

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

bool SwitcherData::checkPause()
{
	bool pause = false;
	obs_source_t* currentSource = obs_frontend_get_current_scene();
	obs_weak_source_t* ws = obs_source_get_weak_source(currentSource);

	for (OBSWeakSource& s : pauseScenesSwitches)
	{
		if (s == ws)
		{
			pause = true;
			break;
		}
	}
	obs_source_release(currentSource);
	obs_weak_source_release(ws);

	string title;
	if (!pause)
	{
		//lock.unlock();
		GetCurrentWindowTitle(title);
		//lock.lock();
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
		//lock.unlock();
		GetCurrentWindowTitle(title);
		//lock.lock();
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
	return pause;
}
