#include "macro-condition-media.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "plugin-state-helpers.hpp"
#include "scene-switch-helpers.hpp"
#include "selection-helpers.hpp"
#include "source-helpers.hpp"

namespace advss {

const std::string MacroConditionMedia::id = "media";

bool MacroConditionMedia::_registered = MacroConditionFactory::Register(
	MacroConditionMedia::id,
	{MacroConditionMedia::Create, MacroConditionMediaEdit::Create,
	 "AdvSceneSwitcher.condition.media"});

static const std::map<MacroConditionMedia::Time, std::string>
	mediaTimeRestrictions = {
		{MacroConditionMedia::Time::TIME_RESTRICTION_NONE,
		 "AdvSceneSwitcher.mediaTab.timeRestriction.none"},
		{MacroConditionMedia::Time::TIME_RESTRICTION_SHORTER,
		 "AdvSceneSwitcher.mediaTab.timeRestriction.shorter"},
		{MacroConditionMedia::Time::TIME_RESTRICTION_LONGER,
		 "AdvSceneSwitcher.mediaTab.timeRestriction.longer"},
		{MacroConditionMedia::Time::TIME_RESTRICTION_REMAINING_SHORTER,
		 "AdvSceneSwitcher.mediaTab.timeRestriction.remainShorter"},
		{MacroConditionMedia::Time::TIME_RESTRICTION_REMAINING_LONGER,
		 "AdvSceneSwitcher.mediaTab.timeRestriction.remainLonger"},
};

static const std::map<MacroConditionMedia::State, std::string> mediaStates = {
	{MacroConditionMedia::State::OBS_MEDIA_STATE_NONE,
	 "AdvSceneSwitcher.mediaTab.states.none"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_PLAYING,
	 "AdvSceneSwitcher.mediaTab.states.playing"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_OPENING,
	 "AdvSceneSwitcher.mediaTab.states.opening"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_BUFFERING,
	 "AdvSceneSwitcher.mediaTab.states.buffering"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_PAUSED,
	 "AdvSceneSwitcher.mediaTab.states.paused"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_STOPPED,
	 "AdvSceneSwitcher.mediaTab.states.stopped"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_ENDED,
	 "AdvSceneSwitcher.mediaTab.states.ended"},
	{MacroConditionMedia::State::OBS_MEDIA_STATE_ERROR,
	 "AdvSceneSwitcher.mediaTab.states.error"},
	{MacroConditionMedia::State::PLAYLIST_ENDED,
	 "AdvSceneSwitcher.mediaTab.states.playlistEnd"},
	{MacroConditionMedia::State::ANY,
	 "AdvSceneSwitcher.mediaTab.states.any"},
};

bool MacroConditionMedia::CheckTime()
{
	auto s = OBSGetStrongRef(_source.GetSource());
	auto duration = obs_source_media_get_duration(s);
	auto currentTime = obs_source_media_get_time(s);

	bool match = false;

	switch (_timeRestriction) {
	case Time::TIME_RESTRICTION_NONE:
		match = true;
		break;
	case Time::TIME_RESTRICTION_SHORTER:
		match = currentTime < _time.Milliseconds();
		break;
	case Time::TIME_RESTRICTION_LONGER:
		match = currentTime > _time.Milliseconds();
		break;
	case Time::TIME_RESTRICTION_REMAINING_SHORTER:
		match = duration > currentTime &&
			duration - currentTime < _time.Milliseconds();
		break;
	case Time::TIME_RESTRICTION_REMAINING_LONGER:
		match = duration > currentTime &&
			duration - currentTime > _time.Milliseconds();
		break;
	default:
		break;
	}

	SetTempVarValues(s, MediaTimeInfo{duration, currentTime});

	return match;
}

bool MacroConditionMedia::CheckState()
{
	auto s = OBSGetStrongRef(_source.GetSource());
	obs_media_state currentState = obs_source_media_get_state(s);

	bool match = false;
	// To be able to compare to obs_media_state more easily
	int expectedState = static_cast<int>(_state);

	switch (_state) {
	case State::OBS_MEDIA_STATE_STOPPED:
		match = _stopped || currentState == OBS_MEDIA_STATE_STOPPED;
		break;
	case State::OBS_MEDIA_STATE_ENDED:
		match = _ended || currentState == OBS_MEDIA_STATE_ENDED;
		break;
	case State::PLAYLIST_ENDED:
		match = CheckPlaylistEnd(currentState);
		break;
	case State::ANY:
		match = true;
		break;
	case State::OBS_MEDIA_STATE_NONE:
	case State::OBS_MEDIA_STATE_PLAYING:
	case State::OBS_MEDIA_STATE_OPENING:
	case State::OBS_MEDIA_STATE_BUFFERING:
	case State::OBS_MEDIA_STATE_PAUSED:
	case State::OBS_MEDIA_STATE_ERROR:
		match = currentState == expectedState;
		break;
	default:
		break;
	}

	SetTempVarValues(s, currentState);

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
	if (!_source.GetSource()) {
		return false;
	}

	bool matched = false;
	switch (_checkType) {
	case CheckType::STATE:
		matched = CheckState();
		break;
	case CheckType::TIME:
		matched = CheckTime();
		break;
	case CheckType::LEGACY:
		matched = CheckState() && CheckTime();
		break;
	default:
		break;
	}

	// reset for next check
	_stopped = false;
	_ended = false;
	_next = false;

	return matched;
}

void MacroConditionMedia::HandleSceneChange()
{
	UpdateMediaSourcesOfSceneList();
	_lastConfigureScene = GetCurrentScene();
}

static bool isVLCSource(obs_source_t *source)
{
	return source &&
	       strcmp(obs_source_get_unversioned_id(source), "vlc_source") == 0;
}

void MacroConditionMedia::SetupTempVars()
{
	MacroCondition::SetupTempVars();

	if (_sourceType != SourceType::SOURCE) {
		return;
	}

	if (_checkType == CheckType::STATE) {
		AddTempvar(
			"state",
			obs_module_text("AdvSceneSwitcher.tempVar.media.state"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.media.state.description"));
	} else if (_checkType == CheckType::TIME) {
		AddTempvar(
			"time",
			obs_module_text("AdvSceneSwitcher.tempVar.media.time"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.media.time.description"));
		AddTempvar(
			"duration",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.media.duration"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.media.duration.description"));
	}

	auto source = OBSGetStrongRef(_source.GetSource());
	if (!isVLCSource(source)) {
		return;
	}

#define CREATE_VLC_TEMPVAR(name)                                             \
	AddTempvar(                                                          \
		name,                                                        \
		obs_module_text("AdvSceneSwitcher.tempVar.media.vlc." name), \
		obs_module_text(                                             \
			"AdvSceneSwitcher.tempVar.media.vlc.metadata.description"));

	CREATE_VLC_TEMPVAR("title")
	CREATE_VLC_TEMPVAR("artist")
	CREATE_VLC_TEMPVAR("genre")
	CREATE_VLC_TEMPVAR("copyright")
	CREATE_VLC_TEMPVAR("album")
	CREATE_VLC_TEMPVAR("track_number")
	CREATE_VLC_TEMPVAR("description")
	CREATE_VLC_TEMPVAR("rating")
	CREATE_VLC_TEMPVAR("date")
	CREATE_VLC_TEMPVAR("setting")
	CREATE_VLC_TEMPVAR("url")
	CREATE_VLC_TEMPVAR("language")
	CREATE_VLC_TEMPVAR("now_playing")
	CREATE_VLC_TEMPVAR("publisher")
	CREATE_VLC_TEMPVAR("encoded_by")
	CREATE_VLC_TEMPVAR("artwork_url")
	CREATE_VLC_TEMPVAR("track_id")
	CREATE_VLC_TEMPVAR("director")
	CREATE_VLC_TEMPVAR("season")
	CREATE_VLC_TEMPVAR("episode")
	CREATE_VLC_TEMPVAR("show_name")
	CREATE_VLC_TEMPVAR("actors")
	CREATE_VLC_TEMPVAR("album_artist")
	CREATE_VLC_TEMPVAR("disc_number")
	CREATE_VLC_TEMPVAR("disc_total")
#undef CREATE_TEMPVAR
}

void MacroConditionMedia::SetTempVarValues(
	obs_source_t *source,
	std::variant<obs_media_state, MediaTimeInfo> value)
{
	if (std::holds_alternative<obs_media_state>(value)) {
		const auto state = std::get<obs_media_state>(value);
		SetTempVarValue("state", std::to_string(state));
	} else {
		const auto timeInfo = std::get<MediaTimeInfo>(value);
		SetTempVarValue("time", std::to_string(timeInfo.time));
		SetTempVarValue("duration", std::to_string(timeInfo.duration));
	}

	if (!isVLCSource(source)) {
		return;
	}

	SetVLCTempVarValueHelper(source, "title");
	SetVLCTempVarValueHelper(source, "artist");
	SetVLCTempVarValueHelper(source, "genre");
	SetVLCTempVarValueHelper(source, "copyright");
	SetVLCTempVarValueHelper(source, "album");
	SetVLCTempVarValueHelper(source, "track_number");
	SetVLCTempVarValueHelper(source, "description");
	SetVLCTempVarValueHelper(source, "rating");
	SetVLCTempVarValueHelper(source, "date");
	SetVLCTempVarValueHelper(source, "setting");
	SetVLCTempVarValueHelper(source, "url");
	SetVLCTempVarValueHelper(source, "language");
	SetVLCTempVarValueHelper(source, "now_playing");
	SetVLCTempVarValueHelper(source, "publisher");
	SetVLCTempVarValueHelper(source, "encoded_by");
	SetVLCTempVarValueHelper(source, "artwork_url");
	SetVLCTempVarValueHelper(source, "track_id");
	SetVLCTempVarValueHelper(source, "director");
	SetVLCTempVarValueHelper(source, "season");
	SetVLCTempVarValueHelper(source, "episode");
	SetVLCTempVarValueHelper(source, "show_name");
	SetVLCTempVarValueHelper(source, "actors");
	SetVLCTempVarValueHelper(source, "album_artist");
	SetVLCTempVarValueHelper(source, "disc_number");
	SetVLCTempVarValueHelper(source, "disc_total");
}

void MacroConditionMedia::SetVLCTempVarValueHelper(obs_source_t *source,
						   const char *id)
{
	auto ph = obs_source_get_proc_handler(source);
	auto cd = calldata_create();
	calldata_set_string(cd, "tag_id", id);
	if (!proc_handler_call(ph, "get_metadata", cd)) {
		SetTempVarValue(id, "");
		calldata_destroy(cd);
		return;
	}
	const char *value;
	if (!calldata_get_string(cd, "tag_data", &value) || !value) {
		SetTempVarValue(id, "");
		calldata_destroy(cd);
		return;
	}
	SetTempVarValue(id, value);
	calldata_destroy(cd);
}

MacroConditionMedia::MacroConditionMedia(const MacroConditionMedia &other)
	: MacroCondition(other.GetMacro()),
	  _sourceType(other._sourceType),
	  _checkType(other._checkType),
	  _state(other._state),
	  _timeRestriction(other._timeRestriction),
	  _scene(other._scene),
	  _source(other._source),
	  _sourceGroup(),
	  _time(other._time),
	  _lastConfigureScene(other._lastConfigureScene)
{
	ResetSignalHandler();
}

MacroConditionMedia &
MacroConditionMedia::operator=(const MacroConditionMedia &other)
{
	_sourceType = other._sourceType;
	_checkType = other._checkType;
	_state = other._state;
	_timeRestriction = other._timeRestriction;
	_scene = other._scene;
	_source = other._source;
	_time = other._time;
	_lastConfigureScene = other._lastConfigureScene;

	ResetSignalHandler();

	return *this;
}

bool MacroConditionMedia::CheckCondition()
{
	bool match = false;
	switch (_sourceType) {
	case SourceType::ANY:
		for (auto &source : _sourceGroup) {
			match = match || source.CheckCondition();
		}
		break;
	case SourceType::ALL: {
		bool res = true;
		for (auto &source : _sourceGroup) {
			res = res && source.CheckCondition();
		}
		match = res;
		break;
	}
	case SourceType::SOURCE:
		match = CheckMediaMatch();
		break;
	default:
		break;
	}

	if (_lastConfigureScene != GetCurrentScene()) {
		HandleSceneChange();
	}

	return match;
}

bool MacroConditionMedia::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_source.Save(obj);
	_scene.Save(obj);
	obs_data_set_int(obj, "sourceType", static_cast<int>(_sourceType));
	obs_data_set_int(obj, "checkType", static_cast<int>(_checkType));
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	obs_data_set_int(obj, "restriction",
			 static_cast<int>(_timeRestriction));
	_time.Save(obj);
	obs_data_set_int(obj, "version", 1);
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
	if (IsMediaSource(source)) {
		OBSWeakSourceAutoRelease weak =
			obs_source_get_weak_source(source);
		sources->emplace_back(weak);
	}
	return true;
}

void MacroConditionMedia::UpdateMediaSourcesOfSceneList()
{
	_sourceGroup.clear();
	if (!_scene.GetScene(false)) {
		return;
	}
	std::vector<OBSWeakSource> mediaSources;
	OBSSourceAutoRelease s =
		obs_weak_source_get_source(_scene.GetScene(false));
	auto scene = obs_scene_from_source(s);
	obs_scene_enum_items(scene, enumSceneItem, &mediaSources);
	_sourceGroup.reserve(mediaSources.size());

	for (auto &source : mediaSources) {
		MacroConditionMedia cond(*this);
		cond._sourceType = SourceType::SOURCE;
		cond._source.SetSource(source);
		_sourceGroup.push_back(cond);
	}
}

void MacroConditionMedia::SetSource(const SourceSelection &source)
{
	_source = source;
	SetupTempVars();
}

bool MacroConditionMedia::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_scene.Load(obj);
	_sourceType =
		static_cast<SourceType>(obs_data_get_int(obj, "sourceType"));
	_state = static_cast<State>(obs_data_get_int(obj, "state"));
	_checkType = static_cast<CheckType>(obs_data_get_int(obj, "checkType"));
	_timeRestriction =
		static_cast<Time>(obs_data_get_int(obj, "restriction"));
	_time.Load(obj);

	if (_sourceType == SourceType::SOURCE) {
		ResetSignalHandler();
	}

	UpdateMediaSourcesOfSceneList();
	if (!obs_data_has_user_value(obj, "version")) {
		if (_state == State::OBS_MEDIA_STATE_ENDED) {
			_state = State::PLAYLIST_ENDED;
		}
	}
	if (obs_data_get_int(obj, "version") < 1) {
		if (IsUsingLegacyCheck()) {
			_checkType = CheckType::LEGACY;
		} else if (_state == State::ANY) {
			_checkType = CheckType::TIME;
		} else {
			_checkType = CheckType::STATE;
		}
	}

	SetupTempVars();

	return true;
}

std::string MacroConditionMedia::GetShortDesc() const
{
	switch (_sourceType) {
	case SourceType::SOURCE:
		return _source.ToString();
	case SourceType::ANY:
		if (_scene.GetScene(false)) {
			return obs_module_text(
				       "AdvSceneSwitcher.condition.media.anyOnScene") +
			       std::string(" ") + _scene.ToString();
		}
		break;
	case SourceType::ALL:
		if (_scene.GetScene(false)) {
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

bool MacroConditionMedia::IsUsingLegacyCheck() const
{
	return _state != State::ANY &&
	       _timeRestriction != Time::TIME_RESTRICTION_NONE;
}

void MacroConditionMedia::ResetSignalHandler()
{
	_signals.clear();
	OBSSourceAutoRelease mediasource =
		obs_weak_source_get_source(_source.GetSource());
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	_signals.emplace_back(sh, "media_stopped", MediaStopped, this);
	_signals.emplace_back(sh, "media_ended", MediaEnded, this);
	_signals.emplace_back(sh, "media_next", MediaNext, this);
}

void MacroConditionMedia::MediaStopped(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (MacroIsPaused(macro)) {
		return;
	}
	media->_stopped = true;
}

void MacroConditionMedia::MediaEnded(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (MacroIsPaused(macro)) {
		return;
	}
	media->_ended = true;
}

void MacroConditionMedia::MediaNext(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (MacroIsPaused(macro)) {
		return;
	}
	media->_next = true;
}

void MacroConditionMedia::SetSourceType(SourceType t)
{
	SetupTempVars();
	_sourceType = t;
}

void MacroConditionMedia::SetCheckType(CheckType t)
{
	SetupTempVars();
	_checkType = t;
}

static void populateSateSelection(QComboBox *list, bool addLegacyEntries)
{
	for (const auto &[value, name] : mediaStates) {
		if (value == MacroConditionMedia::State::ANY &&
		    !addLegacyEntries) {
			continue;
		}
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static void populateTimeRestrictionSelection(QComboBox *list,
					     bool addLegacyEntries)
{
	for (const auto &[value, name] : mediaTimeRestrictions) {
		if (value == MacroConditionMedia::Time::TIME_RESTRICTION_NONE &&
		    !addLegacyEntries) {
			continue;
		}
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static void populateSourceTypes(QComboBox *list)
{
	list->clear();
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.source"),
		static_cast<int>(MacroConditionMedia::SourceType::SOURCE));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.anyOnScene"),
		static_cast<int>(MacroConditionMedia::SourceType::ANY));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.allOnScene"),
		static_cast<int>(MacroConditionMedia::SourceType::ALL));
}

static void populateCheckTypes(QComboBox *list)
{
	list->clear();
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.condition.media.checkType.state"),
		static_cast<int>(MacroConditionMedia::CheckType::STATE));
	list->addItem(
		obs_module_text(
			"AdvSceneSwitcher.condition.media.checkType.time"),
		static_cast<int>(MacroConditionMedia::CheckType::TIME));
}

MacroConditionMediaEdit::MacroConditionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMedia> entryData)
	: QWidget(parent),
	  _sourceTypes(new QComboBox()),
	  _checkTypes(new QComboBox()),
	  _scenes(new SceneSelectionWidget(window(), true, true, true, true,
					   true)),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _states(new QComboBox()),
	  _timeRestrictions(new QComboBox()),
	  _time(new DurationSelection())
{
	_states->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.media.inconsistencyInfo"));

	auto sources = GetMediaSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_sourceTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(SourceTypeChanged(int)));
	QWidget::connect(_checkTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CheckTypeChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_states, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(StateChanged(int)));
	QWidget::connect(_timeRestrictions, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(TimeRestrictionChanged(int)));
	QWidget::connect(_time, SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(TimeChanged(const Duration &)));

	populateSourceTypes(_sourceTypes);
	populateCheckTypes(_checkTypes);
	const bool isUsingLegacyCheck = entryData->GetCheckType() ==
					MacroConditionMedia::CheckType::LEGACY;
	populateSateSelection(_states, isUsingLegacyCheck);
	populateTimeRestrictionSelection(_timeRestrictions, isUsingLegacyCheck);

	auto layout = new QHBoxLayout;
	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sourceTypes}}", _sourceTypes},
		{"{{checkTypes}}", _checkTypes},
		{"{{mediaSources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{states}}", _states},
		{"{{timeRestrictions}}", _timeRestrictions},
		{"{{time}}", _time},
	};
	PlaceWidgets(
		obs_module_text(
			isUsingLegacyCheck
				? "AdvSceneSwitcher.condition.media.layout.legacy"
				: "AdvSceneSwitcher.condition.media.layout"),
		layout, widgetPlaceholders);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionMediaEdit::SourceTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetSourceType(static_cast<MacroConditionMedia::SourceType>(
		_sourceTypes->itemData(idx).toInt()));

	if (_entryData->GetSourceType() ==
	    MacroConditionMedia::SourceType::SOURCE) {
		_entryData->_sourceGroup.clear();
	}

	_entryData->ResetSignalHandler();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));

	SetWidgetVisibility();
}

void MacroConditionMediaEdit::CheckTypeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCheckType(static_cast<MacroConditionMedia::CheckType>(
		_checkTypes->itemData(idx).toInt()));
	SetWidgetVisibility();
}

void MacroConditionMediaEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_sourceGroup.clear();
	_entryData->SetSourceType(MacroConditionMedia::SourceType::SOURCE);
	_entryData->SetSource(source);
	_entryData->ResetSignalHandler();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	SetWidgetVisibility();
}

void MacroConditionMediaEdit::SceneChanged(const SceneSelection &s)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_scene = s;
	_entryData->UpdateMediaSourcesOfSceneList();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionMediaEdit::StateChanged(int index)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_state = static_cast<MacroConditionMedia::State>(
		_states->itemData(index).toInt());
	if (_entryData->GetSourceType() !=
	    MacroConditionMedia::SourceType::SOURCE) {
		_entryData->UpdateMediaSourcesOfSceneList();
	}
}

void MacroConditionMediaEdit::TimeRestrictionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	const auto timeRestriction = static_cast<MacroConditionMedia::Time>(
		_timeRestrictions->itemData(index).toInt());
	if (timeRestriction ==
	    MacroConditionMedia::Time::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	} else {
		_time->setDisabled(false);
	}

	auto lock = LockContext();
	_entryData->_timeRestriction = timeRestriction;
	if (_entryData->GetSourceType() !=
	    MacroConditionMedia::SourceType::SOURCE) {
		_entryData->UpdateMediaSourcesOfSceneList();
	}
}

void MacroConditionMediaEdit::TimeChanged(const Duration &dur)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_time = dur;
	if (_entryData->GetSourceType() !=
	    MacroConditionMedia::SourceType::SOURCE) {
		_entryData->UpdateMediaSourcesOfSceneList();
	}
}

void MacroConditionMediaEdit::SetWidgetVisibility()
{
	_sources->setVisible(_entryData->GetSourceType() ==
			     MacroConditionMedia::SourceType::SOURCE);
	_scenes->setVisible(_entryData->GetSourceType() !=
			    MacroConditionMedia::SourceType::SOURCE);

	if (_entryData->GetCheckType() ==
	    MacroConditionMedia::CheckType::LEGACY) {
		_checkTypes->hide();
		return;
	}

	_states->setVisible(_entryData->GetCheckType() ==
			    MacroConditionMedia::CheckType::STATE);

	_timeRestrictions->setVisible(_entryData->GetCheckType() ==
				      MacroConditionMedia::CheckType::TIME);
	_time->setVisible(_entryData->GetCheckType() ==
			  MacroConditionMedia::CheckType::TIME);

	adjustSize();
	updateGeometry();
}

void MacroConditionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_checkTypes->setCurrentIndex(_checkTypes->findData(
		static_cast<int>(_entryData->GetCheckType())));
	_sourceTypes->setCurrentIndex(_sourceTypes->findData(
		static_cast<int>(_entryData->GetSourceType())));
	_sources->SetSource(_entryData->GetSource());
	_scenes->SetScene(_entryData->_scene);
	_states->setCurrentIndex(
		_states->findData(static_cast<int>(_entryData->_state)));
	_timeRestrictions->setCurrentIndex(_timeRestrictions->findData(
		static_cast<int>(_entryData->_timeRestriction)));
	_time->SetDuration(_entryData->_time);
	if (_entryData->_timeRestriction ==
	    MacroConditionMedia::Time::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	}
	SetWidgetVisibility();
}

} // namespace advss
