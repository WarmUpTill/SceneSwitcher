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

	std::lock_guard<std::mutex> lock(switcher->m);
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

	std::lock_guard<std::mutex> lock(switcher->m);
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

	std::string mediaStr =
		item->data(Qt::UserRole).toString().toUtf8().constData();
	{
		std::lock_guard<std::mutex> lock(switcher->m);
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

void SwitcherData::saveMediaSwitches(obs_data_t *obj)
{
	obs_data_array_t *mediaArray = obs_data_array_create();
	for (MediaSwitch &s : switcher->mediaSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *source = obs_weak_source_get_source(s.source);
		obs_source_t *sceneSource = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		if ((s.usePreviousScene || sceneSource) && source &&
		    transition) {
			const char *sourceName = obs_source_get_name(source);
			const char *sceneName =
				obs_source_get_name(sceneSource);
			const char *transitionName =
				obs_source_get_name(transition);
			obs_data_set_string(array_obj, "source", sourceName);
			obs_data_set_string(array_obj, "scene",
					    s.usePreviousScene
						    ? PREVIOUS_SCENE_NAME
						    : sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_int(array_obj, "state", s.state);
			obs_data_set_int(array_obj, "restriction",
					 s.restriction);
			obs_data_set_int(array_obj, "time", s.time);
			obs_data_array_push_back(mediaArray, array_obj);
		}
		obs_source_release(source);
		obs_source_release(sceneSource);
		obs_source_release(transition);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "mediaSwitches", mediaArray);
	obs_data_array_release(mediaArray);
}

void SwitcherData::loadMediaSwitches(obs_data_t *obj)
{
	obs_data_array_t *mediaArray = obs_data_get_array(obj, "mediaSwitches");
	switcher->mediaSwitches.clear();
	size_t count = obs_data_array_count(mediaArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(mediaArray, i);

		const char *source = obs_data_get_string(array_obj, "source");
		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		obs_media_state state =
			(obs_media_state)obs_data_get_int(array_obj, "state");
		time_restriction restriction =
			(time_restriction)obs_data_get_int(array_obj,
							   "restriction");
		uint64_t time = obs_data_get_int(array_obj, "time");

		std::string mediaStr = MakeMediaSwitchName(source, scene,
							   transition, state,
							   restriction, time)
					       .toUtf8()
					       .constData();

		switcher->mediaSwitches.emplace_back(
			GetWeakSourceByName(scene), GetWeakSourceByName(source),
			GetWeakTransitionByName(transition), state, restriction,
			time, (strcmp(scene, PREVIOUS_SCENE_NAME) == 0),
			mediaStr);

		obs_data_release(array_obj);
	}
	obs_data_array_release(mediaArray);
}
