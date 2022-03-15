#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-media.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionMedia::id = "media";

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
	{MediaState::PLAYLIST_ENDED,
	 "AdvSceneSwitcher.mediaTab.states.playlistEnd"},
	{MediaState::ANY, "AdvSceneSwitcher.mediaTab.states.any"},
};

MacroConditionMedia::~MacroConditionMedia()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
	obs_source_release(mediasource);
}

bool MacroConditionMedia::CheckTime()
{
	obs_source_t *s = obs_weak_source_get_source(_source);
	auto duration = obs_source_media_get_duration(s);
	auto currentTime = obs_source_media_get_time(s);
	obs_source_release(s);

	bool match = false;

	switch (_restriction) {
	case MediaTimeRestriction::TIME_RESTRICTION_NONE:
		match = true;
		break;
	case MediaTimeRestriction::TIME_RESTRICTION_SHORTER:
		match = currentTime < _time.seconds * 1000;
		break;
	case MediaTimeRestriction::TIME_RESTRICTION_LONGER:
		match = currentTime > _time.seconds * 1000;
		break;
	case MediaTimeRestriction::TIME_RESTRICTION_REMAINING_SHORTER:
		match = duration > currentTime &&
			duration - currentTime < _time.seconds * 1000;
		break;
	case MediaTimeRestriction::TIME_RESTRICTION_REMAINING_LONGER:
		match = duration > currentTime &&
			duration - currentTime > _time.seconds * 1000;
		break;
	default:
		break;
	}

	return match;
}

bool MacroConditionMedia::CheckState()
{
	obs_source_t *s = obs_weak_source_get_source(_source);
	obs_media_state currentState = obs_source_media_get_state(s);
	obs_source_release(s);

	bool match = false;
	// To be able to compare to obs_media_state more easily
	int expectedState = static_cast<int>(_state);

	switch (_state) {
	case MediaState::OBS_MEDIA_STATE_STOPPED:
		match = _stopped || currentState == OBS_MEDIA_STATE_STOPPED;
		break;
	case MediaState::OBS_MEDIA_STATE_ENDED:
		match = _ended || currentState == OBS_MEDIA_STATE_ENDED;
		break;
	case MediaState::PLAYLIST_ENDED:
		match = CheckPlaylistEnd(currentState);
		break;
	case MediaState::ANY:
		match = true;
		break;
	case MediaState::OBS_MEDIA_STATE_NONE:
	case MediaState::OBS_MEDIA_STATE_PLAYING:
	case MediaState::OBS_MEDIA_STATE_OPENING:
	case MediaState::OBS_MEDIA_STATE_BUFFERING:
	case MediaState::OBS_MEDIA_STATE_PAUSED:
	case MediaState::OBS_MEDIA_STATE_ERROR:
		match = currentState == expectedState;
		break;
	default:
		break;
	}

	return match;
}

bool MacroConditionMedia::CheckPlaylistEnd(const obs_media_state currentState)
{
	bool consecutiveEndedStates = false;
	if (_next || currentState != OBS_MEDIA_STATE_ENDED) {
		_previousStateEnded = false;
	}
	if (currentState == OBS_MEDIA_STATE_ENDED && _previousStateEnded) {
		consecutiveEndedStates = true;
	}
	_previousStateEnded = _ended || currentState == OBS_MEDIA_STATE_ENDED;
	return consecutiveEndedStates;
}

bool MacroConditionMedia::CheckMediaMatch()
{
	if (!_source) {
		return false;
	}
	bool match = false;
	bool matched = CheckState() && CheckTime();

	if (matched && !(_onlyMatchonChagne && _alreadyMatched)) {
		match = true;
	}
	_alreadyMatched = matched;

	// reset for next check
	_stopped = false;
	_ended = false;
	_next = false;

	return match;
}

bool MacroConditionMedia::CheckCondition()
{
	bool match = false;
	switch (_sourceType) {
	case MediaSourceType::ANY:
		for (auto &source : _sources) {
			match = match || source.CheckCondition();
		}
		break;
	case MediaSourceType::ALL: {
		bool res = true;
		for (auto &source : _sources) {
			res = res && source.CheckCondition();
		}
		match = res;
		break;
	}
	case MediaSourceType::SOURCE:
		match = CheckMediaMatch();
		break;
	default:
		break;
	}
	return match;
}

bool MacroConditionMedia::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "source", GetWeakSourceName(_source).c_str());
	_scene.Save(obj);
	obs_data_set_int(obj, "sourceType", static_cast<int>(_sourceType));
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	obs_data_set_int(obj, "restriction", static_cast<int>(_restriction));
	_time.Save(obj);
	obs_data_set_bool(obj, "matchOnChagne", _onlyMatchonChagne);
	obs_data_set_int(obj, "version", 0);
	return true;
}

static bool enumSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *ptr)
{
	auto *sources = reinterpret_cast<std::vector<OBSWeakSource> *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_scene_t *scene = obs_sceneitem_group_get_scene(item);
		obs_scene_enum_items(scene, enumSceneItem, ptr);
	}

	auto source = obs_sceneitem_get_source(item);
	std::string sourceId = obs_source_get_id(source);

	if (sourceId.compare("ffmpeg_source") == 0 ||
	    sourceId.compare("vlc_source") == 0) {
		auto ws = obs_source_get_weak_source(source);
		obs_weak_source_release(ws);
		sources->emplace_back(ws);
	}
	return true;
}

void forMediaSourceOnSceneAddMediaCondition(
	OBSWeakSource sceneWeakSource, MacroConditionMedia *origCond,
	std::vector<MacroConditionMedia> &conditions)
{
	conditions.clear();
	std::vector<OBSWeakSource> mediaSources;
	auto s = obs_weak_source_get_source(sceneWeakSource);
	auto scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, enumSceneItem, &mediaSources);
	obs_source_release(s);

	for (auto &source : mediaSources) {
		MacroConditionMedia cond(*origCond);
		cond._sourceType = MediaSourceType::SOURCE;
		cond._source = source;
		conditions.push_back(cond);
	}
}

bool MacroConditionMedia::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *sourceName = obs_data_get_string(obj, "source");
	_source = GetWeakSourceByName(sourceName);
	_scene.Load(obj);
	_sourceType = static_cast<MediaSourceType>(
		obs_data_get_int(obj, "sourceType"));
	_state = static_cast<MediaState>(obs_data_get_int(obj, "state"));
	_restriction = static_cast<MediaTimeRestriction>(
		obs_data_get_int(obj, "restriction"));
	_time.Load(obj);
	_onlyMatchonChagne = obs_data_get_bool(obj, "matchOnChagne");

	if (_sourceType == MediaSourceType::SOURCE) {
		obs_source_t *mediasource = obs_weak_source_get_source(_source);
		signal_handler_t *sh =
			obs_source_get_signal_handler(mediasource);
		signal_handler_connect(sh, "media_stopped", MediaStopped, this);
		signal_handler_connect(sh, "media_ended", MediaEnded, this);
		signal_handler_connect(sh, "media_next", MediaNext, this);
		obs_source_release(mediasource);
	}

	forMediaSourceOnSceneAddMediaCondition(_scene.GetScene(), this,
					       _sources);
	if (!obs_data_has_user_value(obj, "version")) {
		if (_state == MediaState::OBS_MEDIA_STATE_ENDED) {
			_state = MediaState::PLAYLIST_ENDED;
		}
	}
	return true;
}

std::string MacroConditionMedia::GetShortDesc()
{
	switch (_sourceType) {
	case MediaSourceType::SOURCE:
		if (_source) {
			return GetWeakSourceName(_source);
		}
		break;
	case MediaSourceType::ANY:
		if (_scene.GetScene()) {
			return obs_module_text(
				       "AdvSceneSwitcher.condition.media.anyOnScene") +
			       std::string(" ") + _scene.ToString();
		}
		break;
	case MediaSourceType::ALL:
		if (_scene.GetScene()) {
			return obs_module_text(
				       "AdvSceneSwitcher.condition.media.allOnScene") +
			       std::string(" ") + _scene.ToString();
		}
		break;
	default:
		break;
	}
	return "";
}

void MacroConditionMedia::ClearSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
	obs_source_release(mediasource);
}

void MacroConditionMedia::ResetSignalHandler()
{
	obs_source_t *mediasource = obs_weak_source_get_source(_source);
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	signal_handler_connect(sh, "media_next", MediaNext, this);
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

void MacroConditionMedia::MediaNext(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	media->_next = true;
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

static void addAnyAndAllStates(QComboBox *list)
{
	list->insertItem(
		1,
		obs_module_text("AdvSceneSwitcher.condition.media.anyOnScene"));
	list->insertItem(
		1,
		obs_module_text("AdvSceneSwitcher.condition.media.allOnScene"));
}

MacroConditionMediaEdit::MacroConditionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMedia> entryData)
	: QWidget(parent),
	  _mediaSources(new QComboBox()),
	  _scenes(new SceneSelectionWidget(window())),
	  _states(new QComboBox()),
	  _timeRestrictions(new QComboBox()),
	  _time(new DurationSelection()),
	  _onChange(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.media.matchOnChange")))

{
	_states->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.media.inconsistencyInfo"));

	QWidget::connect(_mediaSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(_timeRestrictions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(TimeRestrictionChanged(int)));
	QWidget::connect(_time, SIGNAL(DurationChanged(double)), this,
			 SLOT(TimeChanged(double)));
	QWidget::connect(_time, SIGNAL(UnitChanged(DurationUnit)), this,
			 SLOT(TimeUnitChanged(DurationUnit)));
	QWidget::connect(_onChange, SIGNAL(stateChanged(int)), this,
			 SLOT(OnChangeChanged(int)));

	populateMediaSelection(_mediaSources);
	addAnyAndAllStates(_mediaSources);
	populateMediaStates(_states);
	populateMediaTimeRestrictions(_timeRestrictions);

	QHBoxLayout *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{mediaSources}}", _mediaSources},
		{"{{scenes}}", _scenes},
		{"{{states}}", _states},
		{"{{timeRestrictions}}", _timeRestrictions},
		{"{{time}}", _time},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.media.entry"),
		     entryLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_onChange);
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

	if (text ==
	    obs_module_text("AdvSceneSwitcher.condition.media.anyOnScene")) {
		_entryData->_sourceType = MediaSourceType::ANY;
	} else if (text ==
		   obs_module_text(
			   "AdvSceneSwitcher.condition.media.allOnScene")) {
		_entryData->_sourceType = MediaSourceType::ALL;
	} else {
		_entryData->_sources.clear();
		_entryData->_sourceType = MediaSourceType::SOURCE;
	}

	_entryData->ClearSignalHandler();
	_entryData->_source = GetWeakSourceByQString(text);
	_entryData->ResetSignalHandler();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));

	SetWidgetVisibility();
}

void MacroConditionMediaEdit::SceneChanged(const SceneSelection &s)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_scene = s;
	forMediaSourceOnSceneAddMediaCondition(_entryData->_scene.GetScene(),
					       _entryData.get(),
					       _entryData->_sources);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
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
	if (_entryData->_sourceType != MediaSourceType::SOURCE) {
		forMediaSourceOnSceneAddMediaCondition(
			_entryData->_scene.GetScene(), _entryData.get(),
			_entryData->_sources);
	}
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
	if (_entryData->_sourceType != MediaSourceType::SOURCE) {
		forMediaSourceOnSceneAddMediaCondition(
			_entryData->_scene.GetScene(), _entryData.get(),
			_entryData->_sources);
	}
}

void MacroConditionMediaEdit::TimeChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_time.seconds = seconds;
	if (_entryData->_sourceType != MediaSourceType::SOURCE) {
		forMediaSourceOnSceneAddMediaCondition(
			_entryData->_scene.GetScene(), _entryData.get(),
			_entryData->_sources);
	}
}

void MacroConditionMediaEdit::TimeUnitChanged(DurationUnit unit)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_time.displayUnit = unit;
	if (_entryData->_sourceType != MediaSourceType::SOURCE) {
		forMediaSourceOnSceneAddMediaCondition(
			_entryData->_scene.GetScene(), _entryData.get(),
			_entryData->_sources);
	}
}

void MacroConditionMediaEdit::OnChangeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_onlyMatchonChagne = value;
	if (_entryData->_sourceType != MediaSourceType::SOURCE) {
		forMediaSourceOnSceneAddMediaCondition(
			_entryData->_scene.GetScene(), _entryData.get(),
			_entryData->_sources);
	}
}

void MacroConditionMediaEdit::SetWidgetVisibility()
{
	_scenes->setVisible(_entryData->_sourceType != MediaSourceType::SOURCE);
	if (!_onChange->isChecked()) {
		_onChange->hide();
	}
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

	switch (_entryData->_sourceType) {
	case MediaSourceType::ANY:
		_mediaSources->setCurrentText(obs_module_text(
			"AdvSceneSwitcher.condition.media.anyOnScene"));
		break;
	case MediaSourceType::ALL:
		_mediaSources->setCurrentText(obs_module_text(
			"AdvSceneSwitcher.condition.media.allOnScene"));
		break;
	case MediaSourceType::SOURCE:
		_mediaSources->setCurrentText(
			GetWeakSourceName(_entryData->_source).c_str());
		break;
	default:
		break;
	}

	_scenes->SetScene(_entryData->_scene);
	_states->setCurrentIndex(getIdxFromMediaState(_entryData->_state));
	_timeRestrictions->setCurrentIndex(
		static_cast<int>(_entryData->_restriction));
	_time->SetDuration(_entryData->_time);
	if (_entryData->_restriction ==
	    MediaTimeRestriction::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	}
	_onChange->setChecked(_entryData->_onlyMatchonChagne);
	SetWidgetVisibility();
}
