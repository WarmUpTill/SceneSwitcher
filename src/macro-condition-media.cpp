#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-media.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const int MacroConditionMedia::id = 5;

bool MacroConditionMedia::_registered = MacroConditionFactory::Register(
	MacroConditionMedia::id,
	{MacroConditionMedia::Create, MacroConditionMediaEdit::Create,
	 "AdvSceneSwitcher.condition.media"});

static std::map<MediaTimeRestriction, std::string> mediaTimeRestrictions = {
	{MediaTimeRestriction::TIME_RESTRICTION_NONE,
	 "AdvSceneSwitcher.mediaTab.timeRestriction.none"},
	{MediaTimeRestriction::TIME_RESTRICTION_SHORTER,
	 "AdvSceneSwitcher.mediaTab.timeRestriction.shorter"},
	{MediaTimeRestriction::TIME_RESTRICTION_LONGER,
	 "AdvSceneSwitcher.mediaTab.timeRestriction.longer"},
	{MediaTimeRestriction::TIME_RESTRICTION_REMAINING_SHORTER,
	 "AdvSceneSwitcher.mediaTab.timeRestriction.remainShorter"},
	{MediaTimeRestriction::TIME_RESTRICTION_REMAINING_LONGER,
	 "AdvSceneSwitcher.mediaTab.timeRestriction.remainLonger"},
};

static std::map<MediaState, std::string> mediaStates = {
	{MediaState::OBS_MEDIA_STATE_NONE,
	 "AdvSceneSwitcher.mediaTab.states.none"},
	{MediaState::OBS_MEDIA_STATE_PLAYING,
	 "AdvSceneSwitcher.mediaTab.states.playing"},
	{MediaState::OBS_MEDIA_STATE_OPENING,
	 "AdvSceneSwitcher.mediaTab.states.opening"},
	{MediaState::OBS_MEDIA_STATE_BUFFERING,
	 "AdvSceneSwitcher.mediaTab.states.buffering"},
	{MediaState::OBS_MEDIA_STATE_PAUSED,
	 "AdvSceneSwitcher.mediaTab.states.paused"},
	{MediaState::OBS_MEDIA_STATE_STOPPED,
	 "AdvSceneSwitcher.mediaTab.states.stopped"},
	{MediaState::OBS_MEDIA_STATE_ENDED,
	 "AdvSceneSwitcher.mediaTab.states.ended"},
	{MediaState::OBS_MEDIA_STATE_ERROR,
	 "AdvSceneSwitcher.mediaTab.states.error"},
	{MediaState::PLAYED_TO_END,
	 "AdvSceneSwitcher.mediaTab.states.playedToEnd"},
	{MediaState::ANY, "AdvSceneSwitcher.mediaTab.states.any"},
};

MacroConditionMedia::~MacroConditionMedia()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

bool matchTime(const int64_t currentTime, const int64_t duration,
	       const MediaTimeRestriction restriction, const int64_t time)
{
	bool matchedTimeNone =
		(restriction == MediaTimeRestriction::TIME_RESTRICTION_NONE);
	bool matchedTimeLonger =
		(restriction ==
		 MediaTimeRestriction::TIME_RESTRICTION_LONGER) &&
		(currentTime > time);
	bool matchedTimeShorter =
		(restriction ==
		 MediaTimeRestriction::TIME_RESTRICTION_SHORTER) &&
		(currentTime < time);
	bool matchedTimeRemainLonger =
		(restriction ==
		 MediaTimeRestriction::TIME_RESTRICTION_REMAINING_LONGER) &&
		(duration > currentTime && duration - currentTime > time);
	bool matchedTimeRemainShorter =
		(restriction ==
		 MediaTimeRestriction::TIME_RESTRICTION_REMAINING_SHORTER) &&
		(duration > currentTime && duration - currentTime < time);

	return matchedTimeNone || matchedTimeLonger || matchedTimeShorter ||
	       matchedTimeRemainLonger || matchedTimeRemainShorter;
}

bool MacroConditionMedia::CheckCondition()
{
	if (!_source) {
		return false;
	}
	bool match = false;

	obs_source_t *source = obs_weak_source_get_source(_source);
	auto duration = obs_source_media_get_duration(source);
	auto time = obs_source_media_get_time(source);
	obs_media_state currentState = obs_source_media_get_state(source);
	obs_source_release(source);

	// To be able to compare to obs_media_state more easily
	int expectedState = static_cast<int>(_state);

	bool matchedStopped = expectedState == OBS_MEDIA_STATE_STOPPED &&
			      _stopped;

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
	// bool matchedEnded = _state == OBS_MEDIA_STATE_ENDED && _ended;

	bool ended = false;

	if (currentState == OBS_MEDIA_STATE_ENDED) {
		ended = _previousStateEnded;
		_previousStateEnded = true;
	} else {
		_previousStateEnded = false;
	}

	bool matchedEnded = ended && (expectedState == OBS_MEDIA_STATE_ENDED);

	// match if playedToEnd was true in last interval
	// and playback is currently ended
	bool matchedPlayedToEnd = _state == MediaState::PLAYED_TO_END &&
				  _playedToEnd && ended;

	// interval * 2 to make sure not to miss any state changes
	// which happened during check of the conditions
	_playedToEnd = _playedToEnd ||
		       (duration - time <= switcher->interval * 2);

	// reset for next check
	if (ended) {
		_playedToEnd = false;
	}
	_stopped = false;
	_ended = false;

	bool matchedState =
		(currentState == expectedState || _state == MediaState::ANY) ||
		matchedStopped || matchedEnded || matchedPlayedToEnd;

	bool matchedTime =
		matchTime(time, duration, _restriction, _time.seconds * 1000);
	bool matched = matchedState && matchedTime;

	if (matched && !_alreadyMatched) {
		match = true;
	}

	_alreadyMatched = matched;

	return match;
}

bool MacroConditionMedia::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());

	obs_data_set_int(obj, "state", static_cast<int>(_state));
	obs_data_set_int(obj, "restriction", static_cast<int>(_restriction));
	_time.Save(obj);
	return true;
}

bool MacroConditionMedia::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);

	_state = static_cast<MediaState>(obs_data_get_int(obj, "state"));
	_restriction = static_cast<MediaTimeRestriction>(
		obs_data_get_int(obj, "restriction"));
	_time.Load(obj);

	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
	return true;
}

void MacroConditionMedia::ClearSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

void MacroConditionMedia::ResetSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	obs_source_release(mediasource);
}

void MacroConditionMedia::MediaStopped(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	media->_stopped = true;
}

void MacroConditionMedia::MediaEnded(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	media->_ended = true;
}

static void populateMediaTimeRestrictions(QComboBox *list)
{
	for (auto entry : mediaTimeRestrictions) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static void populateMediaStates(QComboBox *list)
{
	for (auto entry : mediaStates) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionMediaEdit::MacroConditionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMedia> entryData)
	: QWidget(parent)
{
	_mediaSources = new QComboBox();
	_states = new QComboBox();
	_timeRestrictions = new QComboBox();
	_time = new DurationSelection();

	QWidget::connect(_mediaSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(_timeRestrictions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(TimeRestrictionChanged(int)));
	QWidget::connect(_time, SIGNAL(DurationChanged(double)), this,
			 SLOT(TimeChanged(double)));
	QWidget::connect(_time, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(TimeUnitChanged(DurationUnit)));

	AdvSceneSwitcher::populateMediaSelection(_mediaSources);
	populateMediaStates(_states);
	populateMediaTimeRestrictions(_timeRestrictions);

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{mediaSources}}", _mediaSources},
		{"{{states}}", _states},
		{"{{timeRestrictions}}", _timeRestrictions},
		{"{{time}}", _time},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.media.entry"),
		     mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionMediaEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->ClearSignalHandler();
	_entryData->_source = GetWeakSourceByQString(text);
	_entryData->ResetSignalHandler();
}

MediaState getMediaStateFromIdx(int idx)
{
	if (idx < static_cast<int>(MediaState::LAST_OBS_MEDIA_STATE)) {
		return static_cast<MediaState>(idx);
	} else {
		return static_cast<MediaState>(
			idx -
			static_cast<int>(MediaState::LAST_OBS_MEDIA_STATE) +
			custom_media_states_offset);
	}
}

void MacroConditionMediaEdit::StateChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_state = getMediaStateFromIdx(index);
}

void MacroConditionMediaEdit::TimeRestrictionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	if (static_cast<MediaTimeRestriction>(index) ==
	    MediaTimeRestriction::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	} else {
		_time->setDisabled(false);
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_restriction = static_cast<MediaTimeRestriction>(index);
}

void MacroConditionMediaEdit::TimeChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_time.seconds = seconds;
}

void MacroConditionMediaEdit::TimeUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_time.displayUnit = unit;
}

int getIdxFromMediaState(MediaState state)
{
	if (state < MediaState::LAST_OBS_MEDIA_STATE) {
		return static_cast<int>(state);
	} else {
		return static_cast<int>(state) - custom_media_states_offset +
		       static_cast<int>(MediaState::LAST_OBS_MEDIA_STATE);
	}
}

void MacroConditionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_mediaSources->setCurrentText(
		GetWeakSourceName(_entryData->_source).c_str());
	_states->setCurrentIndex(getIdxFromMediaState(_entryData->_state));
	_timeRestrictions->setCurrentIndex(
		static_cast<int>(_entryData->_restriction));
	_time->SetDuration(_entryData->_time);
	if (_entryData->_restriction ==
	    MediaTimeRestriction::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	}
}
