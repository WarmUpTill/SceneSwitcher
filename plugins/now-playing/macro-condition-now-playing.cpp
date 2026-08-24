#include "macro-condition-now-playing.hpp"
#include "layout-helpers.hpp"
#include "sync-helpers.hpp"

#include <util/base.h>

#include <roapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.Control.h>

namespace advss {

using namespace winrt::Windows::Media::Control;

const std::string MacroConditionNowPlaying::id = "now_playing";

bool MacroConditionNowPlaying::_registered = MacroConditionFactory::Register(
	MacroConditionNowPlaying::id,
	{MacroConditionNowPlaying::Create, MacroConditionNowPlayingEdit::Create,
	 "AdvSceneSwitcher.condition.nowPlaying"});

// ---------------------------------------------------------------------------

struct SessionInfo {
	bool hasSession = false;
	std::string title;
	std::string artist;
	std::string album;
	std::string appName;
	GlobalSystemMediaTransportControlsSessionPlaybackStatus playbackStatus =
		GlobalSystemMediaTransportControlsSessionPlaybackStatus::Closed;
};

static bool initWinRT()
{
	HRESULT hr = RoInitialize(RO_INIT_MULTITHREADED);
	// S_OK: initialized, S_FALSE: already initialized with same type,
	// RPC_E_CHANGED_MODE: thread is STA (OBS UI thread) - WinRT still usable
	return SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
}

static SessionInfo getCurrentSessionInfo()
{
	thread_local bool initialized = initWinRT();
	(void)initialized;

	SessionInfo info;

	try {
		auto manager =
			GlobalSystemMediaTransportControlsSessionManager::
				RequestAsync()
					.get();
		auto session = manager.GetCurrentSession();
		if (!session) {
			return info;
		}

		info.hasSession = true;
		info.appName = winrt::to_string(session.SourceAppUserModelId());

		auto playbackInfo = session.GetPlaybackInfo();
		info.playbackStatus = playbackInfo.PlaybackStatus();

		auto props = session.TryGetMediaPropertiesAsync().get();
		if (props) {
			info.title = winrt::to_string(props.Title());
			info.artist = winrt::to_string(props.Artist());
			info.album = winrt::to_string(props.AlbumTitle());
		}
	} catch (const winrt::hresult_error &e) {
		blog(LOG_WARNING, "now playing: WinRT error 0x%08X",
		     static_cast<uint32_t>(e.code()));
	}

	return info;
}

static std::string PlaybackStatusToString(
	GlobalSystemMediaTransportControlsSessionPlaybackStatus status)
{
	switch (status) {
	case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing:
		return "Playing";
	case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused:
		return "Paused";
	case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Stopped:
		return "Stopped";
	case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Opened:
		return "Opening";
	case GlobalSystemMediaTransportControlsSessionPlaybackStatus::Changing:
		return "Changing";
	default:
		return "Closed";
	}
}

// ---------------------------------------------------------------------------

bool MacroConditionNowPlaying::CheckCondition()
{
	const auto info = getCurrentSessionInfo();

	SetTempVarValue("title", info.title);
	SetTempVarValue("artist", info.artist);
	SetTempVarValue("album", info.album);
	SetTempVarValue("appName", info.appName);
	SetTempVarValue("playbackStatus",
			PlaybackStatusToString(info.playbackStatus));

	if (!info.hasSession) {
		return false;
	}

	auto textMatches = [this](const std::string &field) -> bool {
		const QString pattern = QString::fromStdString(_matchText);
		const QString value = QString::fromStdString(field);
		if (_regex.Enabled()) {
			return _regex.Matches(value, pattern);
		}
		return value == pattern;
	};

	switch (_checkType) {
	case CheckType::PLAYBACK_STATE: {
		using S =
			GlobalSystemMediaTransportControlsSessionPlaybackStatus;
		switch (_playbackState) {
		case PlaybackState::PLAYING:
			return info.playbackStatus == S::Playing;
		case PlaybackState::PAUSED:
			return info.playbackStatus == S::Paused;
		case PlaybackState::STOPPED:
			return info.playbackStatus == S::Stopped;
		case PlaybackState::OPENING:
			return info.playbackStatus == S::Opened;
		case PlaybackState::CHANGING:
			return info.playbackStatus == S::Changing;
		}
		break;
	}
	case CheckType::TITLE:
		return textMatches(info.title);
	case CheckType::ARTIST:
		return textMatches(info.artist);
	case CheckType::ALBUM:
		return textMatches(info.album);
	case CheckType::APP_NAME:
		return textMatches(info.appName);
	}

	return false;
}

bool MacroConditionNowPlaying::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	obs_data_set_int(obj, "checkType", static_cast<int>(_checkType));
	obs_data_set_int(obj, "playbackState",
			 static_cast<int>(_playbackState));
	_matchText.Save(obj, "matchText");
	_regex.Save(obj);
	return true;
}

bool MacroConditionNowPlaying::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_checkType = static_cast<CheckType>(obs_data_get_int(obj, "checkType"));
	_playbackState = static_cast<PlaybackState>(
		obs_data_get_int(obj, "playbackState"));
	_matchText.Load(obj, "matchText");
	_regex.Load(obj);
	return true;
}

void MacroConditionNowPlaying::SetupTempVars()
{
	AddTempvar(
		"title",
		obs_module_text("AdvSceneSwitcher.tempVar.nowPlaying.title"));
	AddTempvar(
		"artist",
		obs_module_text("AdvSceneSwitcher.tempVar.nowPlaying.artist"));
	AddTempvar(
		"album",
		obs_module_text("AdvSceneSwitcher.tempVar.nowPlaying.album"));
	AddTempvar(
		"appName",
		obs_module_text("AdvSceneSwitcher.tempVar.nowPlaying.appName"));
	AddTempvar(
		"playbackStatus",
		obs_module_text(
			"AdvSceneSwitcher.tempVar.nowPlaying.playbackStatus"));
}

// ---------------------------------------------------------------------------

static void populateCheckTypeSelection(QComboBox *list)
{
	static const std::map<MacroConditionNowPlaying::CheckType, std::string> checkTypes = {
		{MacroConditionNowPlaying::CheckType::PLAYBACK_STATE,
		 "AdvSceneSwitcher.condition.nowPlaying.checkType.playbackState"},
		{MacroConditionNowPlaying::CheckType::TITLE,
		 "AdvSceneSwitcher.condition.nowPlaying.checkType.title"},
		{MacroConditionNowPlaying::CheckType::ARTIST,
		 "AdvSceneSwitcher.condition.nowPlaying.checkType.artist"},
		{MacroConditionNowPlaying::CheckType::ALBUM,
		 "AdvSceneSwitcher.condition.nowPlaying.checkType.album"},
		{MacroConditionNowPlaying::CheckType::APP_NAME,
		 "AdvSceneSwitcher.condition.nowPlaying.checkType.appName"},
	};

	for (const auto &[type, name] : checkTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(type));
	}
}

static void populatePlaybackStateSelection(QComboBox *list)
{
	static const std::map<MacroConditionNowPlaying::PlaybackState, std::string> playbackStates = {
		{MacroConditionNowPlaying::PlaybackState::PLAYING,
		 "AdvSceneSwitcher.condition.nowPlaying.playbackState.playing"},
		{MacroConditionNowPlaying::PlaybackState::PAUSED,
		 "AdvSceneSwitcher.condition.nowPlaying.playbackState.paused"},
		{MacroConditionNowPlaying::PlaybackState::STOPPED,
		 "AdvSceneSwitcher.condition.nowPlaying.playbackState.stopped"},
		{MacroConditionNowPlaying::PlaybackState::OPENING,
		 "AdvSceneSwitcher.condition.nowPlaying.playbackState.opening"},
		{MacroConditionNowPlaying::PlaybackState::CHANGING,
		 "AdvSceneSwitcher.condition.nowPlaying.playbackState.changing"},
	};

	for (const auto &[state, name] : playbackStates) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(state));
	}
}

MacroConditionNowPlayingEdit::MacroConditionNowPlayingEdit(
	QWidget *parent, std::shared_ptr<MacroConditionNowPlaying> entryData)
	: QWidget(parent),
	  _checkType(new QComboBox(this)),
	  _playbackState(new QComboBox(this)),
	  _matchText(new VariableLineEdit(this)),
	  _regex(new RegexConfigWidget(this))
{
	populateCheckTypeSelection(_checkType);
	populatePlaybackStateSelection(_playbackState);

	connect(_checkType, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, &MacroConditionNowPlayingEdit::CheckTypeChanged);
	connect(_playbackState,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&MacroConditionNowPlayingEdit::PlaybackStateChanged);
	connect(_matchText, &VariableLineEdit::textEdited, this,
		&MacroConditionNowPlayingEdit::MatchTextChanged);
	connect(_regex, &RegexConfigWidget::RegexConfigChanged, this,
		&MacroConditionNowPlayingEdit::RegexChanged);

	_layout = new QHBoxLayout();
	_layout->addWidget(_checkType);
	_layout->addWidget(_playbackState);
	_layout->addWidget(_matchText);
	_layout->addWidget(_regex);
	setLayout(_layout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionNowPlayingEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_checkType->setCurrentIndex(
		_checkType->findData(static_cast<int>(_entryData->_checkType)));
	_playbackState->setCurrentIndex(_playbackState->findData(
		static_cast<int>(_entryData->_playbackState)));
	_matchText->setText(QString::fromStdString(_entryData->_matchText));
	_regex->SetRegexConfig(_entryData->_regex);

	SetWidgetVisibility();
}

void MacroConditionNowPlayingEdit::CheckTypeChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_checkType =
		static_cast<MacroConditionNowPlaying::CheckType>(
			_checkType->currentData().toInt());
	SetWidgetVisibility();
}

void MacroConditionNowPlayingEdit::PlaybackStateChanged(int)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_playbackState =
		static_cast<MacroConditionNowPlaying::PlaybackState>(
			_playbackState->currentData().toInt());
}

void MacroConditionNowPlayingEdit::MatchTextChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_matchText = text.toStdString();
}

void MacroConditionNowPlayingEdit::RegexChanged(const RegexConfig &regex)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_regex = regex;
}

void MacroConditionNowPlayingEdit::SetWidgetVisibility()
{
	const bool isStateCheck =
		_entryData->_checkType ==
		MacroConditionNowPlaying::CheckType::PLAYBACK_STATE;
	_playbackState->setVisible(isStateCheck);
	_matchText->setVisible(!isStateCheck);
	_regex->setVisible(!isStateCheck);

	if (isStateCheck) {
		AddStretchIfNecessary(_layout);
	} else {
		RemoveStretchIfPresent(_layout);
	}

	adjustSize();
	updateGeometry();
}

} // namespace advss
