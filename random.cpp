#include "advanced-scene-switcher.hpp"

void SceneSwitcher::on_randomScenesList_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem* item = ui->randomScenesList->item(idx);

	QString randomSceneStr = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto& s : switcher->randomSwitches)
	{
		if (randomSceneStr.compare(s.randomSwitchStr.c_str()) == 0)
		{
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName = GetWeakSourceName(s.transition).c_str();
			ui->randomScenes->setCurrentText(sceneName);
			ui->randomSpinBox->setValue(s.delay);
			ui->randomTransitions->setCurrentText(transitionName);
			break;
		}
	}
}

int SceneSwitcher::randomFindByData(const QString& randomStr)
{
	int count = ui->randomScenesList->count();

	for (int i = 0; i < count; i++)
	{
		QListWidgetItem* item = ui->randomScenesList->item(i);
		QString str = item->data(Qt::UserRole).toString();

		if (str == randomStr)
			return i;
	}

	return -1;
}

void SceneSwitcher::on_randomAdd_clicked()
{
	QString sceneName = ui->randomScenes->currentText();
	QString transitionName = ui->randomTransitions->currentText();
	double delay = ui->randomSpinBox->value();


	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);


	QString text = MakeRandomSwitchName(sceneName, transitionName, delay);
	QVariant v = QVariant::fromValue(text);

	int idx = randomFindByData(text);

	if (idx == -1)
	{
		lock_guard<mutex> lock(switcher->m);
		switcher->randomSwitches.emplace_back(
			source, transition, delay, text.toUtf8().constData());

		QListWidgetItem* item = new QListWidgetItem(text, ui->randomScenesList);
		item->setData(Qt::UserRole, v);
	}
	else
	{
		QListWidgetItem* item = ui->randomScenesList->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto& s : switcher->randomSwitches)
			{
				if (s.scene == source)
				{
					s.delay = delay;
					s.transition = transition;
					s.randomSwitchStr = text.toUtf8().constData();;
					break;
				}
			}
		}

		ui->randomScenesList->sortItems();
	}
}


void SceneSwitcher::on_randomRemove_clicked()
{
	QListWidgetItem* item = ui->randomScenesList->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto& switches = switcher->randomSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it)
		{
			auto& s = *it;

			if (s.randomSwitchStr == text)
			{
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}


void SwitcherData::checkRandom(bool& match, OBSWeakSource& scene, OBSWeakSource& transition, int& delay)
{
	if (randomSwitches.size() == 0)
		return;

	vector<RandomSwitch> rs (randomSwitches);
	std::random_shuffle(rs.begin(), rs.end());
	for (RandomSwitch& r : rs)
	{
		if (r.scene == lastRandomScene)
			continue;
		scene = r.scene;
		transition = r.transition;
		delay = (int)r.delay * 1000;
		match = true;
		lastRandomScene = r.scene;
		break;
	}
}
