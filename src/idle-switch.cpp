#include "headers/advanced-scene-switcher.hpp"

void SwitcherData::checkIdleSwitch(bool& match, OBSWeakSource& scene, OBSWeakSource& transition)
{
	if (!idleData.idleEnable)
		return;

	string title;
	bool ignoreIdle = false;
	//lock.unlock();
	GetCurrentWindowTitle(title);
	//lock.lock();

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
		if (idleData.alreadySwitched)
			return;
		scene = (idleData.usePreviousScene) ? previousScene : idleData.scene;
		transition = idleData.transition;
		match = true;
		idleData.alreadySwitched = true;
	}
	else
		idleData.alreadySwitched = false;
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

		UpdateIdleDataTransition(ui->idleTransitions->currentText());
		UpdateIdleDataScene(ui->idleScenes->currentText());
	}
}


void SceneSwitcher::UpdateIdleDataTransition(const QString& name)
{
	obs_weak_source_t* transition = GetWeakTransitionByQString(name);
	switcher->idleData.transition = transition;
}

void SceneSwitcher::UpdateIdleDataScene(const QString& name)
{
	switcher->idleData.usePreviousScene = (name == PREVIOUS_SCENE_NAME);
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
