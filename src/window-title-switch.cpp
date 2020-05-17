#include <obs-module.h>
#include "headers/advanced-scene-switcher.hpp"

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
		switcher->windowSwitches.emplace_back(
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
			for (auto& s : switcher->windowSwitches)
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
		auto& switches = switcher->windowSwitches;

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


void SceneSwitcher::on_switches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->switches->item(idx);

	QString window = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->windowSwitches)
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

void SwitcherData::checkWindowTitleSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition)
{
	string title;

	// Check if current window is ignored
	GetCurrentWindowTitle(title);
	for (auto& window : ignoreWindowsSwitches)
	{
		// True if ignored switch equals title
		bool equals = (title == window);
		// True if ignored switch matches title
		bool matches = QString::fromStdString(title).contains(QRegularExpression(window.c_str()));

		if (equals || matches)
		{
			title = lastTitle;

			break;
		}
	}
	lastTitle = title;

	// Check for match
	for (WindowSceneSwitch& s : windowSwitches)
	{
		// True if window switch equals title
		bool equals = (title == s.window);
		// True if window switch matches title
		bool matches = QString::fromStdString(title).contains(QRegularExpression(s.window.c_str()));
		// True if fullscreen is disabled OR window is fullscreen
		bool fullscreen = (!s.fullscreen || isFullscreen());

		if ((equals || matches) && fullscreen)
		{
			match = true;
			scene = s.scene;
			transition = s.transition;

			break;
		}
	}
}
