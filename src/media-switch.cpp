#include <obs-module.h>
#include "headers/advanced-scene-switcher.hpp"

void SceneSwitcher::on_mediaSwitches_currentRowChanged(int idx)
{
	if (loading)
		return;
	if (idx == -1)
		return;

	QListWidgetItem *item = ui->mediaSwitches->item(idx);

	QString mediaSceneStr = item->data(Qt::UserRole).toString();

	lock_guard<mutex> lock(switcher->m);
	for (auto &s : switcher->mediaSwitches) {
		if (mediaSceneStr.compare(s.mediaSwitchStr.c_str()) == 0) {
			QString sceneName = GetWeakSourceName(s.scene).c_str();
			QString sourceName =
				GetWeakSourceName(s.source).c_str();
			QString transitionName =
				GetWeakSourceName(s.transition).c_str();
			ui->mediaScenes->setCurrentText(sceneName);
			ui->mediaSources->setCurrentText(sourceName);
			ui->mediaTransitions->setCurrentText(transitionName);
			ui->mediaStates->setCurrentIndex(s.state);
			ui->mediaTimeRestrictions->setCurrentIndex(
				s.restriction);
			ui->mediaTime->setValue(s.time);
			break;
		}
	}
}

void SceneSwitcher::on_mediaAdd_clicked()
{
	QString sourceName = ui->mediaSources->currentText();
	QString sceneName = ui->mediaScenes->currentText();
	QString transitionName = ui->mediaTransitions->currentText();
	obs_media_state state =
		(obs_media_state)ui->mediaStates->currentIndex();
	time_restriction restriction =
		(time_restriction)ui->mediaTimeRestrictions->currentIndex();
	uint64_t time = ui->mediaTime->value();

	if (sceneName.isEmpty() || transitionName.isEmpty() ||
	    sourceName.isEmpty())
		return;

	OBSWeakSource source = GetWeakSourceByQString(sourceName);
	OBSWeakSource scene = GetWeakSourceByQString(sceneName);
	OBSWeakSource transition = GetWeakTransitionByQString(transitionName);

	QString switchText = MakeMediaSwitchName(sourceName, sceneName,
						 transitionName, state,
						 restriction, time);
	QVariant v = QVariant::fromValue(switchText);

	QListWidgetItem *item =
		new QListWidgetItem(switchText, ui->mediaSwitches);
	item->setData(Qt::UserRole, v);

	lock_guard<mutex> lock(switcher->m);
	switcher->mediaSwitches.emplace_back(
		scene, source, transition, state, restriction, time,
		(sceneName == QString(PREVIOUS_SCENE_NAME)),
		switchText.toUtf8().constData());
}

void SceneSwitcher::on_mediaRemove_clicked()
{
	QListWidgetItem *item = ui->mediaSwitches->currentItem();
	if (!item)
		return;

	string mediaStr =
		item->data(Qt::UserRole).toString().toUtf8().constData();
	{
		lock_guard<mutex> lock(switcher->m);
		auto &switches = switcher->mediaSwitches;

		for (auto it = switches.begin(); it != switches.end(); ++it) {
			auto &s = *it;

			if (s.mediaSwitchStr == mediaStr) {
				switches.erase(it);
				break;
			}
		}
	}

	delete item;
}

void SwitcherData::checkMediaSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	for (MediaSwitch &mediaSwitch : mediaSwitches) {
		obs_source_t *source =
			obs_weak_source_get_source(mediaSwitch.source);
		auto duration = obs_source_media_get_duration(source);
		auto time = obs_source_media_get_time(source);
		obs_media_state state = obs_source_media_get_state(source);
		bool matched =
			state == mediaSwitch.state &&
			(mediaSwitch.restriction == TIME_RESTRICTION_NONE ||
			 (mediaSwitch.restriction == TIME_RESTRICTION_LONGER &&
			  time > mediaSwitch.time) ||
			 (mediaSwitch.restriction == TIME_RESTRICTION_SHORTER &&
			  time < mediaSwitch.time) ||
			 (mediaSwitch.restriction ==
				  TIME_RESTRICTION_REMAINING_SHORTER &&
			  duration > time &&
			  duration - time < mediaSwitch.time) ||
			 (mediaSwitch.restriction ==
				  TIME_RESTRICTION_REMAINING_LONGER &&
			  duration > time &&
			  duration - time > mediaSwitch.time));
		if (matched && !mediaSwitch.matched) {
			match = true;
			scene = (mediaSwitch.usePreviousScene)
					? previousScene
					: mediaSwitch.scene;
			transition = mediaSwitch.transition;
		}
		mediaSwitch.matched = matched;
		obs_source_release(source);
	}
}
