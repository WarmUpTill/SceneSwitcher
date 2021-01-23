#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool MediaSwitch::pause = false;
static QMetaObject::Connection addPulse;

constexpr auto media_played_to_end_idx = 8;
constexpr auto media_any_idx = 9;

void AdvSceneSwitcher::on_mediaAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->mediaSwitches.emplace_back();

	listAddClicked(ui->mediaSwitches,
		       new MediaSwitchWidget(this,
					     &switcher->mediaSwitches.back()),
		       ui->mediaAdd, &addPulse);
}

void AdvSceneSwitcher::on_mediaRemove_clicked()
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

void AdvSceneSwitcher::on_mediaUp_clicked()
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

void AdvSceneSwitcher::on_mediaDown_clicked()
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
	if (MediaSwitch::pause)
		return;

	for (MediaSwitch &mediaSwitch : mediaSwitches) {
		if (!mediaSwitch.initialized())
			continue;

		obs_source_t *source =
			obs_weak_source_get_source(mediaSwitch.source);
		auto duration = obs_source_media_get_duration(source);
		auto time = obs_source_media_get_time(source);
		obs_media_state state = obs_source_media_get_state(source);
		obs_source_release(source);

		bool matchedStopped = mediaSwitch.state ==
					      OBS_MEDIA_STATE_STOPPED &&
				      mediaSwitch.stopped;

		// The signal for the state ended is intentionally not used here
		// so matchedEnded can be used to specify the end of a VLC playlist
		// by two consequtive matches of OBS_MEDIA_STATE_ENDED
		//
		// This was done to reduce the likelyhood of interpreting a single
		// OBS_MEDIA_STATE_ENDED caught by obs_source_media_get_state()
		// as the end of the playlist of the VLC source, while actually being
		// in the middle of switching to the next item of the playlist
		//
		// If there is a separate obs_media_sate in the future for the
		// "end of playlist reached" signal the line below can be used
		// and an additional check for this new singal can be introduced
		//
		// bool matchedEnded = mediaSwitch.state == OBS_MEDIA_STATE_ENDED &&
		//		    mediaSwitch.ended;

		bool ended = false;

		if (state == OBS_MEDIA_STATE_ENDED) {
			ended = mediaSwitch.previousStateEnded;
			mediaSwitch.previousStateEnded = true;
		} else {
			mediaSwitch.previousStateEnded = false;
		}

		bool matchedEnded =
			ended && (mediaSwitch.state == OBS_MEDIA_STATE_ENDED);

		// match if playedToEnd was true in last interval
		// and playback is currently ended
		bool matchedPlayedToEnd = mediaSwitch.state ==
						  media_played_to_end_idx &&
					  mediaSwitch.playedToEnd && ended;

		// interval * 2 to make sure not to miss any state changes
		// which happened during check of the conditions
		mediaSwitch.playedToEnd = mediaSwitch.playedToEnd ||
					  (duration - time <= interval * 2);

		// reset
		if (ended)
			mediaSwitch.playedToEnd = false;

		// reset for next check
		mediaSwitch.stopped = false;
		mediaSwitch.ended = false;

		bool matchedState = ((state == mediaSwitch.state) ||
				     mediaSwitch.anyState) ||
				    matchedStopped || matchedEnded ||
				    matchedPlayedToEnd;

		bool matchedTimeNone =
			(mediaSwitch.restriction == TIME_RESTRICTION_NONE);
		bool matchedTimeLonger =
			(mediaSwitch.restriction == TIME_RESTRICTION_LONGER) &&
			(time > mediaSwitch.time);
		bool matchedTimeShorter =
			(mediaSwitch.restriction == TIME_RESTRICTION_SHORTER) &&
			(time < mediaSwitch.time);
		bool matchedTimeRemainLonger =
			(mediaSwitch.restriction ==
			 TIME_RESTRICTION_REMAINING_LONGER) &&
			(duration > time && duration - time > mediaSwitch.time);
		bool matchedTimeRemainShorter =
			(mediaSwitch.restriction ==
			 TIME_RESTRICTION_REMAINING_SHORTER) &&
			(duration > time && duration - time < mediaSwitch.time);

		bool matchedTime = matchedTimeNone || matchedTimeLonger ||
				   matchedTimeShorter ||
				   matchedTimeRemainLonger ||
				   matchedTimeRemainShorter;

		bool matched = matchedState && matchedTime;

		if (matched && !mediaSwitch.matched) {
			match = true;
			scene = mediaSwitch.getScene();
			transition = mediaSwitch.transition;

			if (verbose)
				mediaSwitch.logMatch();
		}

		mediaSwitch.matched = matched;

		if (match)
			break;
	}
}

void SwitcherData::saveMediaSwitches(obs_data_t *obj)
{
	obs_data_array_t *mediaArray = obs_data_array_create();
	for (MediaSwitch &s : switcher->mediaSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(mediaArray, array_obj);

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

		switcher->mediaSwitches.emplace_back();
		mediaSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(mediaArray);
}

void AdvSceneSwitcher::setupMediaTab()
{
	for (auto &s : switcher->mediaSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->mediaSwitches);
		ui->mediaSwitches->addItem(item);
		MediaSwitchWidget *sw = new MediaSwitchWidget(this, &s);
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

void MediaSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_source_t *s = obs_weak_source_get_source(source);
	const char *sourceName = obs_source_get_name(s);
	obs_data_set_string(obj, "source", sourceName);
	obs_source_release(s);

	obs_data_set_int(obj, "state", state);
	obs_data_set_int(obj, "restriction", restriction);
	obs_data_set_int(obj, "time", time);
}

// To be removed in future version
bool loadOldMedia(obs_data_t *obj, MediaSwitch *s)
{
	if (!s)
		return false;

	const char *scene = obs_data_get_string(obj, "scene");

	if (strcmp(scene, "") == 0)
		return false;

	s->scene = GetWeakSourceByName(scene);

	const char *transition = obs_data_get_string(obj, "transition");
	s->transition = GetWeakTransitionByName(transition);

	const char *source = obs_data_get_string(obj, "source");
	s->source = GetWeakSourceByName(source);

	s->state = (obs_media_state)obs_data_get_int(obj, "state");
	s->restriction = (time_restriction)obs_data_get_int(obj, "restriction");
	s->time = obs_data_get_int(obj, "time");
	s->usePreviousScene = strcmp(scene, previous_scene_name) == 0;

	return true;
}

void MediaSwitch::load(obs_data_t *obj)
{

	if (loadOldMedia(obj, this))
		return;

	SceneSwitcherEntry::load(obj);

	const char *sourceName = obs_data_get_string(obj, "source");
	source = GetWeakSourceByName(sourceName);

	state = (obs_media_state)obs_data_get_int(obj, "state");
	restriction = (time_restriction)obs_data_get_int(obj, "restriction");
	time = obs_data_get_int(obj, "time");

	anyState = state == media_any_idx;
	obs_source_t *mediasource = obs_weak_source_get_source(source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

void MediaSwitch::clearSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

void MediaSwitch::resetSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

void MediaSwitch::MediaStopped(void *data, calldata_t *)
{
	MediaSwitch *media = static_cast<MediaSwitch *>(data);
	media->stopped = true;
}

void MediaSwitch::MediaEnded(void *data, calldata_t *)
{
	MediaSwitch *media = static_cast<MediaSwitch *>(data);
	media->ended = true;
}

MediaSwitch::MediaSwitch(const MediaSwitch &other)
	: SceneSwitcherEntry(other.scene, other.transition,
			     other.usePreviousScene),
	  source(other.source),
	  state(other.state),
	  restriction(other.restriction),
	  time(other.time)
{
	anyState = state == media_any_idx;
	obs_source_t *mediasource = obs_weak_source_get_source(source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

MediaSwitch::MediaSwitch(MediaSwitch &&other)
	: SceneSwitcherEntry(other.scene, other.transition,
			     other.usePreviousScene),
	  source(other.source),
	  state(other.state),
	  anyState(other.anyState),
	  restriction(other.restriction),
	  time(other.time)
{
}

MediaSwitch::~MediaSwitch()
{
	obs_source_t *mediasource = obs_weak_source_get_source(source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

MediaSwitch &MediaSwitch::operator=(const MediaSwitch &other)
{
	MediaSwitch t(other);
	swap(*this, t);
	return *this = MediaSwitch(other);
}

MediaSwitch &MediaSwitch::operator=(MediaSwitch &&other) noexcept
{
	if (this == &other) {
		return *this;
	}

	swap(*this, other);

	obs_source_t *mediasource = obs_weak_source_get_source(other.source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, &other);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, &other);
	obs_source_release(mediasource);

	return *this;
}

void swap(MediaSwitch &first, MediaSwitch &second)
{
	std::swap(first.scene, second.scene);
	std::swap(first.transition, second.transition);
	std::swap(first.usePreviousScene, second.usePreviousScene);
	std::swap(first.source, second.source);
	std::swap(first.state, second.state);
	std::swap(first.restriction, second.restriction);
	std::swap(first.time, second.time);
	std::swap(first.anyState, second.anyState);
	first.resetSignalHandler();
	second.resetSignalHandler();
}

void populateMediaStates(QComboBox *list)
{
	list->addItem(obs_module_text("AdvSceneSwitcher.mediaTab.states.none"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.playing"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.opening"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.buffering"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.paused"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.stopped"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.ended"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.mediaTab.states.error"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.states.playedToEnd"));
	list->addItem(obs_module_text("AdvSceneSwitcher.mediaTab.states.any"));
}

void populateTimeRestrictions(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.timeRestriction.none"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.timeRestriction.shorter"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.timeRestriction.longer"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.timeRestriction.remainShorter"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.mediaTab.timeRestriction.remainLonger"));
}

MediaSwitchWidget::MediaSwitchWidget(QWidget *parent, MediaSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	mediaSources = new QComboBox();
	states = new QComboBox();
	timeRestrictions = new QComboBox();
	time = new QSpinBox();

	time->setSuffix("ms");
	time->setMaximum(99999999);
	time->setMinimum(0);

	QWidget::connect(mediaSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(timeRestrictions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(TimeRestrictionChanged(int)));
	QWidget::connect(time, SIGNAL(valueChanged(int)), this,
			 SLOT(TimeChanged(int)));

	AdvSceneSwitcher::populateMediaSelection(mediaSources);
	populateMediaStates(states);
	populateTimeRestrictions(timeRestrictions);

	if (s) {
		mediaSources->setCurrentText(
			GetWeakSourceName(s->source).c_str());
		states->setCurrentIndex(s->state);
		timeRestrictions->setCurrentIndex(s->restriction);
		time->setValue(s->time);
		if (s->restriction == TIME_RESTRICTION_NONE)
			time->setDisabled(true);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{mediaSources}}", mediaSources},
		{"{{states}}", states},
		{"{{timeRestrictions}}", timeRestrictions},
		{"{{time}}", time},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.mediaTab.entry"),
		     mainLayout, widgetPlaceholders);
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
	switchData->clearSignalHandler();
	switchData->source = GetWeakSourceByQString(text);
	switchData->resetSignalHandler();
}

void MediaSwitchWidget::StateChanged(int index)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->state = (obs_media_state)index;
	switchData->anyState = switchData->state == media_any_idx;
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
