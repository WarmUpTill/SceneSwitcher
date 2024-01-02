#include "macro-condition-media.hpp"
#include "macro.hpp"
#include "plugin-state-helpers.hpp"
#include "scene-switch-helpers.hpp"
#include "utility.hpp"

namespace advss {

const std::string MacroConditionMedia::id = "media";

bool MacroConditionMedia::_registered = MacroConditionFactory::Register(
	MacroConditionMedia::id,
	{MacroConditionMedia::Create, MacroConditionMediaEdit::Create,
	 "AdvSceneSwitcher.condition.media"});

static std::map<MacroConditionMedia::Time, std::string> mediaTimeRestrictions = {
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

static std::map<MacroConditionMedia::State, std::string> mediaStates = {
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

MacroConditionMedia::~MacroConditionMedia()
{
	OBSSourceAutoRelease mediasource =
		obs_weak_source_get_source(_source.GetSource());
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
}

bool MacroConditionMedia::CheckTime()
{
	OBSSourceAutoRelease s =
		obs_weak_source_get_source(_source.GetSource());
	auto duration = obs_source_media_get_duration(s);
	auto currentTime = obs_source_media_get_time(s);

	bool match = false;

	switch (_restriction) {
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

	return match;
}

bool MacroConditionMedia::CheckState()
{
	OBSSourceAutoRelease s =
		obs_weak_source_get_source(_source.GetSource());
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
	bool matched = CheckState() && CheckTime();

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

bool MacroConditionMedia::CheckCondition()
{
	bool match = false;
	switch (_sourceType) {
	case Type::ANY:
		for (auto &source : _sourceGroup) {
			match = match || source.CheckCondition();
		}
		break;
	case Type::ALL: {
		bool res = true;
		for (auto &source : _sourceGroup) {
			res = res && source.CheckCondition();
		}
		match = res;
		break;
	}
	case Type::SOURCE:
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
	obs_data_set_int(obj, "state", static_cast<int>(_state));
	obs_data_set_int(obj, "restriction", static_cast<int>(_restriction));
	_time.Save(obj);
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
		cond._sourceType = Type::SOURCE;
		cond._source.SetSource(source);
		_sourceGroup.push_back(cond);
	}
}

bool MacroConditionMedia::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_source.Load(obj);
	_scene.Load(obj);
	_sourceType = static_cast<Type>(obs_data_get_int(obj, "sourceType"));
	_state = static_cast<MacroConditionMedia::State>(
		obs_data_get_int(obj, "state"));
	_restriction = static_cast<MacroConditionMedia::Time>(
		obs_data_get_int(obj, "restriction"));
	_time.Load(obj);

	if (_sourceType == Type::SOURCE) {
		OBSSourceAutoRelease mediasource =
			obs_weak_source_get_source(_source.GetSource());
		signal_handler_t *sh =
			obs_source_get_signal_handler(mediasource);
		signal_handler_connect(sh, "media_stopped", MediaStopped, this);
		signal_handler_connect(sh, "media_ended", MediaEnded, this);
		signal_handler_connect(sh, "media_next", MediaNext, this);
	}

	UpdateMediaSourcesOfSceneList();
	if (!obs_data_has_user_value(obj, "version")) {
		if (_state ==
		    MacroConditionMedia::State::OBS_MEDIA_STATE_ENDED) {
			_state = MacroConditionMedia::State::PLAYLIST_ENDED;
		}
	}
	return true;
}

std::string MacroConditionMedia::GetShortDesc() const
{
	switch (_sourceType) {
	case Type::SOURCE:
		return _source.ToString();
	case Type::ANY:
		if (_scene.GetScene(false)) {
			return obs_module_text(
				       "AdvSceneSwitcher.condition.media.anyOnScene") +
			       std::string(" ") + _scene.ToString();
		}
		break;
	case Type::ALL:
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

void MacroConditionMedia::ClearSignalHandler()
{
	OBSSourceAutoRelease mediasource =
		obs_weak_source_get_source(_source.GetSource());
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
}

void MacroConditionMedia::ResetSignalHandler()
{
	OBSSourceAutoRelease mediasource =
		obs_weak_source_get_source(_source.GetSource());
	signal_handler_t *sh = obs_source_get_signal_handler(mediasource);
	signal_handler_disconnect(sh, "media_stopped", MediaStopped, this);
	signal_handler_disconnect(sh, "media_ended", MediaEnded, this);
	signal_handler_disconnect(sh, "media_next", MediaNext, this);
	signal_handler_connect(sh, "media_stopped", MediaStopped, this);
	signal_handler_connect(sh, "media_ended", MediaEnded, this);
	signal_handler_connect(sh, "media_next", MediaNext, this);
}

void MacroConditionMedia::MediaStopped(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}
	media->_stopped = true;
}

void MacroConditionMedia::MediaEnded(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}
	media->_ended = true;
}

void MacroConditionMedia::MediaNext(void *data, calldata_t *)
{
	MacroConditionMedia *media = static_cast<MacroConditionMedia *>(data);
	const auto macro = media->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}
	media->_next = true;
}

static void populateMediaTimes(QComboBox *list)
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

static void populateSourceTypes(QComboBox *list)
{
	list->clear();
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.source"),
		static_cast<int>(MacroConditionMedia::Type::SOURCE));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.anyOnScene"),
		static_cast<int>(MacroConditionMedia::Type::ANY));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.condition.media.allOnScene"),
		static_cast<int>(MacroConditionMedia::Type::ALL));
}

MacroConditionMediaEdit::MacroConditionMediaEdit(
	QWidget *parent, std::shared_ptr<MacroConditionMedia> entryData)
	: QWidget(parent),
	  _sourceTypes(new QComboBox()),
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
	populateMediaStates(_states);
	populateMediaTimes(_timeRestrictions);

	auto layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{sourceTypes}}", _sourceTypes},
		{"{{mediaSources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{states}}", _states},
		{"{{timeRestrictions}}", _timeRestrictions},
		{"{{time}}", _time},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.media.entry"),
		     layout, widgetPlaceholders);
	setLayout(layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionMediaEdit::SourceTypeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_sourceType = static_cast<MacroConditionMedia::Type>(
		_sourceTypes->itemData(idx).toInt());

	if (_entryData->_sourceType == MacroConditionMedia::Type::SOURCE) {
		_entryData->_sourceGroup.clear();
	}

	_entryData->ResetSignalHandler();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));

	SetWidgetVisibility();
}

void MacroConditionMediaEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_sourceGroup.clear();
	_entryData->_sourceType = MacroConditionMedia::Type::SOURCE;
	_entryData->ClearSignalHandler();
	_entryData->_source = source;
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

	auto lock = LockContext();
	_entryData->_scene = s;
	_entryData->UpdateMediaSourcesOfSceneList();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

MacroConditionMedia::State getMediaStateFromIdx(int idx)
{
	if (idx < static_cast<int>(
			  MacroConditionMedia::State::LAST_OBS_MEDIA_STATE)) {
		return static_cast<MacroConditionMedia::State>(idx);
	} else {
		return static_cast<MacroConditionMedia::State>(
			idx -
			static_cast<int>(
				MacroConditionMedia::State::LAST_OBS_MEDIA_STATE) +
			custom_media_states_offset);
	}
}

void MacroConditionMediaEdit::StateChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_state = getMediaStateFromIdx(index);
	if (_entryData->_sourceType != MacroConditionMedia::Type::SOURCE) {
		_entryData->UpdateMediaSourcesOfSceneList();
	}
}

void MacroConditionMediaEdit::TimeRestrictionChanged(int index)
{
	if (_loading || !_entryData) {
		return;
	}

	if (static_cast<MacroConditionMedia::Time>(index) ==
	    MacroConditionMedia::Time::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	} else {
		_time->setDisabled(false);
	}

	auto lock = LockContext();
	_entryData->_restriction =
		static_cast<MacroConditionMedia::Time>(index);
	if (_entryData->_sourceType != MacroConditionMedia::Type::SOURCE) {
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
	if (_entryData->_sourceType != MacroConditionMedia::Type::SOURCE) {
		_entryData->UpdateMediaSourcesOfSceneList();
	}
}

void MacroConditionMediaEdit::SetWidgetVisibility()
{
	_sources->setVisible(_entryData->_sourceType ==
			     MacroConditionMedia::Type::SOURCE);
	_scenes->setVisible(_entryData->_sourceType !=
			    MacroConditionMedia::Type::SOURCE);
}

static int getIdxFromMediaState(MacroConditionMedia::State state)
{
	if (state < MacroConditionMedia::State::LAST_OBS_MEDIA_STATE) {
		return static_cast<int>(state);
	} else {
		return static_cast<int>(state) - custom_media_states_offset +
		       static_cast<int>(
			       MacroConditionMedia::State::LAST_OBS_MEDIA_STATE);
	}
}

void MacroConditionMediaEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sourceTypes->setCurrentIndex(_sourceTypes->findData(
		static_cast<int>(_entryData->_sourceType)));
	_sources->SetSource(_entryData->_source);
	_scenes->SetScene(_entryData->_scene);
	_states->setCurrentIndex(getIdxFromMediaState(_entryData->_state));
	_timeRestrictions->setCurrentIndex(
		static_cast<int>(_entryData->_restriction));
	_time->SetDuration(_entryData->_time);
	if (_entryData->_restriction ==
	    MacroConditionMedia::Time::TIME_RESTRICTION_NONE) {
		_time->setDisabled(true);
	}
	SetWidgetVisibility();
}

} // namespace advss
