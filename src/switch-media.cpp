#include "headers/advanced-scene-switcher.hpp"

static QMetaObject::Connection addPulse;

void SceneSwitcher::on_mediaAdd_clicked()
{
	ui->mediaAdd->disconnect(addPulse);

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->mediaSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->mediaSwitches);
	ui->mediaSwitches->addItem(item);
	MediaSwitchWidget *sw =
		new MediaSwitchWidget(&switcher->mediaSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->mediaSwitches->setItemWidget(item, sw);
}

void SceneSwitcher::on_mediaRemove_clicked()
{
	QListWidgetItem *item = ui->mediaSwitches->currentItem();
	if (!item)
		return;

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->mediaSwitches->currentRow();
		auto &switches = switcher->mediaSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void SceneSwitcher::on_mediaUp_clicked()
{
	int index = ui->mediaSwitches->currentRow();
	if (!listMoveUp(ui->mediaSwitches))
		return;

	MediaSwitchWidget *s1 =
		(MediaSwitchWidget *)ui->mediaSwitches->itemWidget(
			ui->mediaSwitches->item(index));
	MediaSwitchWidget *s2 =
		(MediaSwitchWidget *)ui->mediaSwitches->itemWidget(
			ui->mediaSwitches->item(index - 1));
	MediaSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->mediaSwitches[index],
		  switcher->mediaSwitches[index - 1]);
}

void SceneSwitcher::on_mediaDown_clicked()
{
	int index = ui->mediaSwitches->currentRow();

	if (!listMoveDown(ui->mediaSwitches))
		return;

	MediaSwitchWidget *s1 =
		(MediaSwitchWidget *)ui->mediaSwitches->itemWidget(
			ui->mediaSwitches->item(index));
	MediaSwitchWidget *s2 =
		(MediaSwitchWidget *)ui->mediaSwitches->itemWidget(
			ui->mediaSwitches->item(index + 1));
	MediaSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->mediaSwitches[index],
		  switcher->mediaSwitches[index + 1]);
}

void SwitcherData::checkMediaSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	for (MediaSwitch &mediaSwitch : mediaSwitches) {
		if (!mediaSwitch.initialized())
			continue;
		OBSSource source =
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
		if (matched) {
			match = true;
			scene = (mediaSwitch.usePreviousScene)
					? previousScene
					: mediaSwitch.scene;
			transition = mediaSwitch.transition;

			if (verbose)
				mediaSwitch.logMatch();
			break;
		}
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
						    ? previous_scene_name
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

		switcher->mediaSwitches.emplace_back(
			GetWeakSourceByName(scene), GetWeakSourceByName(source),
			GetWeakTransitionByName(transition), state, restriction,
			time, (strcmp(scene, previous_scene_name) == 0));

		obs_data_release(array_obj);
	}
	obs_data_array_release(mediaArray);
}

void SceneSwitcher::setupMediaTab()
{
	for (auto &s : switcher->mediaSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->mediaSwitches);
		ui->mediaSwitches->addItem(item);
		MediaSwitchWidget *sw = new MediaSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->mediaSwitches->setItemWidget(item, sw);
	}

	if (switcher->mediaSwitches.size() == 0)
		addPulse = PulseWidget(ui->mediaAdd, QColor(Qt::green));
}

bool MediaSwitch::initialized()
{
	return SceneSwitcherEntry::initialized() && source;
}

bool MediaSwitch::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(source));
}

void populateMediaStates(QComboBox *list)
{
	list->addItem("None");
	list->addItem("Playing");
	list->addItem("Opening");
	list->addItem("Buffering");
	list->addItem("Paused");
	list->addItem("Stopped");
	list->addItem("Ended");
	list->addItem("Error");
}

void populateTimeRestrictions(QComboBox *list)
{
	list->addItem("None");
	list->addItem("Time shorter");
	list->addItem("Time longer");
	list->addItem("Time remaining shorter");
	list->addItem("Time remaining longer");
}

MediaSwitchWidget::MediaSwitchWidget(MediaSwitch *s) : SwitchWidget(s)
{
	meidaSources = new QComboBox();
	states = new QComboBox();
	timeRestrictions = new QComboBox();
	time = new QSpinBox();

	whenLabel = new QLabel("When");
	stateLabel = new QLabel("state is");
	andLabel = new QLabel("and");
	switchLabel = new QLabel("switch to");
	usingLabel = new QLabel("using");

	time->setSuffix("ms");
	time->setMaximum(99999999);
	time->setMinimum(0);

	QWidget::connect(meidaSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(timeRestrictions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(TimeRestrictionChanged(int)));
	QWidget::connect(time, SIGNAL(valueChanged(int)), this,
			 SLOT(TimeChanged(int)));

	SceneSwitcher::populateMediaSelection(meidaSources);
	populateMediaStates(states);
	populateTimeRestrictions(timeRestrictions);

	if (s) {
		meidaSources->setCurrentText(
			GetWeakSourceName(s->source).c_str());
		states->setCurrentIndex(s->state);
		timeRestrictions->setCurrentIndex(s->restriction);
		time->setValue(s->time);
		if (s->restriction == TIME_RESTRICTION_NONE)
			time->setDisabled(true);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *mainLayout = new QHBoxLayout;

	mainLayout->addWidget(whenLabel);
	mainLayout->addWidget(meidaSources);
	mainLayout->addWidget(stateLabel);
	mainLayout->addWidget(states);
	mainLayout->addWidget(andLabel);
	mainLayout->addWidget(timeRestrictions);
	mainLayout->addWidget(time);
	mainLayout->addWidget(switchLabel);
	mainLayout->addWidget(scenes);
	mainLayout->addWidget(usingLabel);
	mainLayout->addWidget(transitions);
	mainLayout->addStretch();

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

MediaSwitch *MediaSwitchWidget::getSwitchData()
{
	return switchData;
}

void MediaSwitchWidget::setSwitchData(MediaSwitch *s)
{
	switchData = s;
}

void MediaSwitchWidget::swapSwitchData(MediaSwitchWidget *s1,
				       MediaSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	MediaSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void MediaSwitchWidget::SourceChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->source = GetWeakSourceByQString(text);
}

void MediaSwitchWidget::StateChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->state = (obs_media_state)index;
}

void MediaSwitchWidget::TimeRestrictionChanged(int index)
{
	if (loading || !switchData)
		return;

	if ((time_restriction)index == TIME_RESTRICTION_NONE)
		time->setDisabled(true);
	else
		time->setDisabled(false);

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->restriction = (time_restriction)index;
}

void MediaSwitchWidget::TimeChanged(int time)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->time = time;
}
