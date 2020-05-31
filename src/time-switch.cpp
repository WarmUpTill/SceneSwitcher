#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_timeSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->timeSwitches->item(idx);

	QString timeScenestr = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->timeSwitches) {
		if (timeScenestr.compare(s.timeSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->timeScenes->setCurrentText(sceneName);
			ui->timeEdit->setTime(s.time);
			ui->timeTransitions->setCurrentText(transitionName);
			break;
		}
	}
}

int SceneSwitcher::timeFindByData(const QString &timeStr)
{
	QRegExp rx("At " + timeStr + " switch to .*");
	int count = ui->timeSwitches->count();

	for (int i = 0; i < count; i++) {
		QListWidgetItem *item = ui->timeSwitches->item(i);
		QString str = item->data(Qt::UserRole).toString();

		if (rx.exactMatch(str))
			return i;
	}

	return -1;
}

void SceneSwitcher::on_timeAdd_clicked()
{
	QString sceneName = ui->timeScenes->currentText();
	QString transitionName = ui->timeTransitions->currentText();
	QTime time = ui->timeEdit->time();

	if (sceneName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString text = MakeTimeSwitchName(sceneName, transitionName, time);
	QVariant v = QVariant::fromValue(text);

	int idx = timeFindByData(time.toString());

	if (idx == -1) {
		lock_guard<mutex> lock(switcher->m);
		switcher->timeSwitches.emplace_back(
			source, transition, time,
			(sceneName == QString(PREVIOUS_SCENE_NAME)),
			text.toUtf8().constData());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->timeSwitches);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->timeSwitches->item(idx);
		item->setText(text);

		{
			lock_guard<mutex> lock(switcher->m);
			for (auto &s : switcher->timeSwitches) {
				if (s.time == time) {
					s.scene = source;
					s.transition = transition;
					s.usePreviousScene =
						(sceneName ==
						 QString(PREVIOUS_SCENE_NAME));
					s.timeSwitchStr =
						text.toUtf8().constData();
					;
					break;
				}
			}
		}

		ui->timeSwitches->sortItems();
	}
}

void SceneSwitcher::on_timeRemove_clicked()
{
	QListWidgetItem *item = ui->timeSwitches->currentItem();
	if (!item)
		return;

	string text = item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->timeSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.timeSwitchStr == text) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SwitcherData::checkTimeSwitch(bool &match, OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (timeSwitches.size() == 0)
		return;

	QTime now = QTime::currentTime();

	for (TimeSwitch &s : timeSwitches) {

		QTime validSwitchTimeWindow = s.time.addMSecs(interval);

		match = s.time <= now && now <= validSwitchTimeWindow;
		if (!match &&
		    validSwitchTimeWindow.msecsSinceStartOfDay() < interval) {
			// check for overflow
			match = now >= s.time || now <= validSwitchTimeWindow;
		}

		if (match) {
			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;
			break;
		}
	}
}
