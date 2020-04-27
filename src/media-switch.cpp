#include <obs-module.h>
#include "headers/advanced-scene-switcher.hpp"

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
		(sceneName == QString(PREVIOUS_SCENE_NAME)));
}

void SceneSwitcher::on_mediaRemove_clicked()
{
	QListWidgetItem *item = ui->mediaSwitches->currentItem();
	if (!item)
		return;

	int idx = ui->mediaSwitches->currentRow();
	if (idx == -1)
		return;

	{
		lock_guard<mutex> lock(switcher->m);

		auto &switches = switcher->mediaSwitches;
		switches.erase(switches.begin() + idx);
	}
	qDeleteAll(ui->mediaSwitches->selectedItems());
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
	}
}
