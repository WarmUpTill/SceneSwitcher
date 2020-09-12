#include "headers/advanced-scene-switcher.hpp"

static std::vector<std::pair<std::string, timeTrigger>> triggerTable = {
	{"On any day", timeTrigger::ANY_DAY},
	{"Mondays", timeTrigger::MONDAY},
	{"Tuesdays", timeTrigger::TUSEDAY},
	{"Wednesdays", timeTrigger::WEDNESDAY},
	{"Thursdays", timeTrigger::THURSDAY},
	{"Fridays", timeTrigger::FRIDAY},
	{"Saturdays", timeTrigger::SATURDAY},
	{"Sundays", timeTrigger::SUNDAY},
	{"Atfer streaming/recording start", timeTrigger::LIVE}};

void SceneSwitcher::on_timeSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->timeSwitches->item(idx);

	QString timeScenestr = item->data(Qt::UserRole).toString();

	std::lock_guard<std::mutex> lock(switcher->m);
	for (auto &s : switcher->timeSwitches) {
		if (timeScenestr.compare(s.timeSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->timeScenes->setCurrentText(sceneName);
			ui->timeTrigger->setCurrentIndex(s.trigger);
			ui->timeEdit->setTime(s.time);
			ui->timeTransitions->setCurrentText(transitionName);
			break;
		}
	}
}

int SceneSwitcher::timeFindByData(const timeTrigger &trigger, const QTime &time)
{
	QRegExp rx(MakeTimeSwitchName(QStringLiteral(".*"),
				      QStringLiteral(".*"), trigger, time));
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

	std::string triggerStr = ui->timeTrigger->currentText().toStdString();
	auto it = std::find_if(
		triggerTable.begin(), triggerTable.end(),
		[&triggerStr](
			const std::pair<std::string, timeTrigger> &element) {
			return element.first == triggerStr;
		});
	timeTrigger trigger = it->second;

	QString text =
		MakeTimeSwitchName(sceneName, transitionName, trigger, time);
	QVariant v = QVariant::fromValue(text);

	int idx = timeFindByData(trigger, time);

	if (idx == -1) {
		std::lock_guard<std::mutex> lock(switcher->m);
		switcher->timeSwitches.emplace_back(
			source, transition, trigger, time,
			(sceneName == QString(PREVIOUS_SCENE_NAME)),
			text.toUtf8().constData());

		QListWidgetItem *item =
			new QListWidgetItem(text, ui->timeSwitches);
		item->setData(Qt::UserRole, v);
	} else {
		QListWidgetItem *item = ui->timeSwitches->item(idx);
		item->setText(text);
		item->setData(Qt::UserRole, v);

		{
			std::lock_guard<std::mutex> lock(switcher->m);
			for (auto &s : switcher->timeSwitches) {
				if (s.trigger == trigger && s.time == time) {
					s.scene = source;
					s.transition = transition;
					s.usePreviousScene =
						(sceneName ==
						 QString(PREVIOUS_SCENE_NAME));
					s.timeSwitchStr =
						text.toUtf8().constData();
					break;
				}
			}
		}
	}
}

void SceneSwitcher::on_timeRemove_clicked()
{
	QListWidgetItem *item = ui->timeSwitches->currentItem();
	if (!item)
		return;

	std::string text =
		item->data(Qt::UserRole).toString().toUtf8().constData();

	{
		std::lock_guard<std::mutex> lock(switcher->m);
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

void SceneSwitcher::on_timeUp_clicked()
{
	int index = ui->timeSwitches->currentRow();
	if (index != -1 && index != 0) {
		ui->timeSwitches->insertItem(index - 1,
					     ui->timeSwitches->takeItem(index));
		ui->timeSwitches->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->timeSwitches.begin() + index,
			  switcher->timeSwitches.begin() + index - 1);
	}
}

void SceneSwitcher::on_timeDown_clicked()
{
	int index = ui->timeSwitches->currentRow();
	if (index != -1 && index != ui->timeSwitches->count() - 1) {
		ui->timeSwitches->insertItem(index + 1,
					     ui->timeSwitches->takeItem(index));
		ui->timeSwitches->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		iter_swap(switcher->timeSwitches.begin() + index,
			  switcher->timeSwitches.begin() + index + 1);
	}
}

bool timesAreInInterval(QTime &time1, QTime &time2, int &interval)
{
	bool ret = false;
	QTime validSwitchTimeWindow = time1.addMSecs(interval);

	ret = time1 <= time2 && time2 <= validSwitchTimeWindow;
	// check for overflow
	if (!ret && validSwitchTimeWindow.msecsSinceStartOfDay() < interval) {
		ret = time2 >= time1 || time2 <= validSwitchTimeWindow;
	}
	return ret;
}

bool checkLiveTime(TimeSwitch &s, QDateTime &start, int &interval)
{
	if (start.isNull())
		return false;

	QDateTime now = QDateTime::currentDateTime();
	QTime timePassed = QTime(0, 0).addMSecs(start.msecsTo(now));

	return timesAreInInterval(s.time, timePassed, interval);
}

bool checkRegularTime(TimeSwitch &s, int &interval)
{
	bool match = false;
	if (s.trigger != ANY_DAY &&
	    s.trigger != QDate::currentDate().dayOfWeek())
		return false;

	QTime now = QTime::currentTime();

	return timesAreInInterval(s.time, now, interval);
}

void SwitcherData::checkTimeSwitch(bool &match, OBSWeakSource &scene,
				   OBSWeakSource &transition)
{
	if (timeSwitches.size() == 0)
		return;

	for (TimeSwitch &s : timeSwitches) {
		if (s.trigger == LIVE)
			match = checkLiveTime(s, liveTime, interval);
		else
			match = checkRegularTime(s, interval);

		if (match) {
			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				blog(LOG_INFO,
				     "Advanced Scene Switcher time match");

			break;
		}
	}
}

void SwitcherData::saveTimeSwitches(obs_data_t *obj)
{
	obs_data_array_t *timeArray = obs_data_array_create();
	for (TimeSwitch &s : switcher->timeSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *sceneSource = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if ((s.usePreviousScene || sceneSource) && transition) {
			const char *sceneName =
				obs_source_get_name(sceneSource);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "scene",
					    s.usePreviousScene
						    ? PREVIOUS_SCENE_NAME
						    : sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_int(array_obj, "trigger", s.trigger);
			obs_data_set_string(
				array_obj, "time",
				s.time.toString().toStdString().c_str());
			obs_data_array_push_back(timeArray, array_obj);
		}
		obs_source_release(sceneSource);
		obs_source_release(transition);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "timeSwitches", timeArray);
	obs_data_array_release(timeArray);
}

void SwitcherData::loadTimeSwitches(obs_data_t *obj)
{
	switcher->timeSwitches.clear();

	obs_data_array_t *timeArray = obs_data_get_array(obj, "timeSwitches");
	size_t count = obs_data_array_count(timeArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(timeArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		timeTrigger trigger =
			(timeTrigger)obs_data_get_int(array_obj, "trigger");
		QTime time = QTime::fromString(
			obs_data_get_string(array_obj, "time"));

		std::string timeSwitchStr =
			MakeTimeSwitchName(scene, transition, trigger, time)
				.toUtf8()
				.constData();

		switcher->timeSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition), trigger, time,
			(strcmp(scene, PREVIOUS_SCENE_NAME) == 0),
			timeSwitchStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(timeArray);
}

void SceneSwitcher::setupTimeTab()
{
	populateSceneSelection(ui->timeScenes, true);
	populateTransitionSelection(ui->timeTransitions);

	for (auto t : triggerTable) {
		ui->timeTrigger->addItem(t.first.c_str());
	}

	// assuming the streaming / recording entry is always last
	ui->timeTrigger->setItemData(
		(int)triggerTable.size() - 1,
		"The time relative to the start of streaming / recording will be used",
		Qt::ToolTipRole);

	for (auto &s : switcher->timeSwitches) {
		std::string sceneName = (s.usePreviousScene)
						? PREVIOUS_SCENE_NAME
						: GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		QString listText = MakeTimeSwitchName(sceneName.c_str(),
						      transitionName.c_str(),
						      s.trigger, s.time);

		QListWidgetItem *item =
			new QListWidgetItem(listText, ui->timeSwitches);
		item->setData(Qt::UserRole, listText);
	}
}
