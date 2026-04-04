#include "macro-action-play-audio.hpp"
#include "audio-helpers.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "sync-helpers.hpp"

#include <chrono>
#include <QFileInfo>
#include <QLabel>

namespace advss {

// Use a high output channel index that is unlikely to be claimed by OBS or
// other plugins. OBS supports channels 0-63; channel 0 is the main scene.
static constexpr uint32_t kPlaybackOutputChannel = 63;

const std::string MacroActionPlayAudio::id = "play_audio";

bool MacroActionPlayAudio::_registered = MacroActionFactory::Register(
	MacroActionPlayAudio::id,
	{MacroActionPlayAudio::Create, MacroActionPlayAudioEdit::Create,
	 "AdvSceneSwitcher.action.playAudio"});

static void deactivatePlayback(obs_source_t *source, bool wantsOutput)
{
	if (wantsOutput) {
		obs_set_output_source(kPlaybackOutputChannel, nullptr);
	} else {
		obs_source_dec_active(source);
	}
}

static void waitForPlaybackToEnd(Macro *macro, obs_source_t *source,
				 int64_t maxMs = 0)
{
	using namespace std::chrono_literals;

	std::unique_lock<std::mutex> lock(*GetMutex());
	SetMacroAbortWait(false);

	// The media source needs time to open and decode before reaching
	// PLAYING state. Poll until it starts (or the macro is stopped).
	while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
		if (obs_source_media_get_state(source) ==
		    OBS_MEDIA_STATE_PLAYING) {
			break;
		}
		GetMacroWaitCV().wait_for(lock, 10ms);
	}

	// Now wait for playback to end. Require two consecutive non-playing
	// samples to avoid false positives on brief state transitions.
	// If maxMs > 0, also stop once that many milliseconds have elapsed.
	const auto playbackStart = std::chrono::steady_clock::now();
	static const int kStopThreshold = 2;
	int stoppedCount = 0;
	while (!MacroWaitShouldAbort() && !MacroIsStopped(macro)) {
		if (maxMs > 0) {
			const auto elapsed =
				std::chrono::duration_cast<
					std::chrono::milliseconds>(
					std::chrono::steady_clock::now() -
					playbackStart)
					.count();
			if (elapsed >= maxMs) {
				obs_source_media_stop(source);
				break;
			}
		}
		if (obs_source_media_get_state(source) !=
		    OBS_MEDIA_STATE_PLAYING) {
			if (++stoppedCount >= kStopThreshold) {
				break;
			}
		} else {
			stoppedCount = 0;
		}
		GetMacroWaitCV().wait_for(lock, 10ms);
	}
}

bool MacroActionPlayAudio::PerformAction()
{
	std::string path = _filePath;
	if (path.empty()) {
		return true;
	}

	if (!QFileInfo::exists(QString::fromStdString(path))) {
		blog(LOG_WARNING, "audio file not found: \"%s\"", path.c_str());
		return true;
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "local_file", path.c_str());
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_bool(settings, "looping", false);
	// Disable automatic restart on activate so we control start explicitly.
	obs_data_set_bool(settings, "restart_on_activate", false);
	obs_data_set_bool(settings, "close_when_inactive", true);

	OBSSourceAutoRelease source = obs_source_create_private(
		"ffmpeg_source", "advss_play_audio", settings);

	if (!source) {
		blog(LOG_WARNING,
		     "Failed to create ffmpeg_source for audio playback of \"%s\"",
		     path.c_str());
		return true;
	}

	const float vol =
		DecibelToPercent(static_cast<float>(_volumeDB.GetValue()));
	obs_source_set_volume(source, vol);
	obs_source_set_monitoring_type(source, _monitorType);

	// Fall back to monitor-only if all output tracks are deselected —
	// there is no point routing through the output channel if the mixer
	// mask would silence every track.
	const bool wantsOutput =
		(_monitorType != OBS_MONITORING_TYPE_MONITOR_ONLY) &&
		(_audioMixers != 0);
	if (!wantsOutput &&
	    _monitorType == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT) {
		obs_source_set_monitoring_type(
			source, OBS_MONITORING_TYPE_MONITOR_ONLY);
	}

	if (wantsOutput) {
		obs_source_set_audio_mixers(source, _audioMixers);
		// Route through a private scene so we can position the scene
		// item far off-screen. This keeps the item "visible" (so audio
		// is still mixed) while ensuring its video never intersects the
		// output frame.
		OBSSceneAutoRelease audioScene =
			obs_scene_create_private("advss_audio_scene");
		obs_sceneitem_t *item = obs_scene_add(audioScene, source);
		if (item) {
			struct vec2 pos = {-99999.0f, -99999.0f};
			obs_sceneitem_set_pos(item, &pos);
		}
		obs_set_output_source(kPlaybackOutputChannel,
				      obs_scene_get_source(audioScene));
		// audioScene released here; the output channel holds the
		// remaining reference and keeps the scene alive.
	} else {
		obs_source_set_audio_mixers(source, 0);
		obs_source_inc_active(source);
	}

	if (_useStartOffset && _startOffset.Milliseconds() > 0) {
		obs_source_media_set_time(source, _startOffset.Milliseconds());
	}

	obs_source_media_play_pause(source, false);

	const int64_t maxMs = _useDuration ? _playbackDuration.Milliseconds()
					   : 0;

	if (_waitForCompletion) {
		SetMacroAbortWait(false);
		{
			SuspendLock suspendLock(*this);
			waitForPlaybackToEnd(GetMacro(), source, maxMs);
		}
		deactivatePlayback(source, wantsOutput);
		return true;
	}

	// Keep the source alive in a background thread that cleans up
	// once playback finishes. Grab an extra strong reference so the
	// source survives beyond this stack frame.
	auto rawSource = obs_source_get_ref(source);
	auto macro = GetMacro();

	std::thread cleanupThread([rawSource, wantsOutput, macro, maxMs]() {
		waitForPlaybackToEnd(macro, rawSource, maxMs);
		deactivatePlayback(rawSource, wantsOutput);
		obs_source_release(rawSource);
	});
	AddMacroHelperThread(macro, std::move(cleanupThread));

	return true;
}

void MacroActionPlayAudio::LogAction() const
{
	ablog(LOG_INFO,
	      "playing audio file \"%s\" at %.1f dB (monitoring type %d, wait %d)",
	      _filePath.UnresolvedValue().c_str(),
	      static_cast<double>(_volumeDB.GetFixedValue()), _monitorType,
	      (int)_waitForCompletion);
}

bool MacroActionPlayAudio::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_filePath.Save(obj, "filePath");
	_volumeDB.Save(obj, "volumeDB");
	obs_data_set_int(obj, "monitorType", _monitorType);
	obs_data_set_int(obj, "audioMixers", _audioMixers);
	obs_data_set_bool(obj, "useStartOffset", _useStartOffset);
	_startOffset.Save(obj, "startOffset");
	obs_data_set_bool(obj, "useDuration", _useDuration);
	_playbackDuration.Save(obj, "playbackDuration");
	obs_data_set_bool(obj, "waitForCompletion", _waitForCompletion);
	return true;
}

bool MacroActionPlayAudio::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_filePath.Load(obj, "filePath");
	_volumeDB.Load(obj, "volumeDB");
	_monitorType = static_cast<obs_monitoring_type>(
		obs_data_get_int(obj, "monitorType"));
	_audioMixers =
		static_cast<uint32_t>(obs_data_get_int(obj, "audioMixers"));
	_useStartOffset = obs_data_get_bool(obj, "useStartOffset");
	_startOffset.Load(obj, "startOffset");
	_useDuration = obs_data_get_bool(obj, "useDuration");
	_playbackDuration.Load(obj, "playbackDuration");
	_waitForCompletion = obs_data_get_bool(obj, "waitForCompletion");
	return true;
}

std::string MacroActionPlayAudio::GetShortDesc() const
{
	return _filePath.UnresolvedValue();
}

std::shared_ptr<MacroAction> MacroActionPlayAudio::Create(Macro *m)
{
	return std::make_shared<MacroActionPlayAudio>(m);
}

std::shared_ptr<MacroAction> MacroActionPlayAudio::Copy() const
{
	return std::make_shared<MacroActionPlayAudio>(*this);
}

void MacroActionPlayAudio::ResolveVariablesToFixedValues()
{
	_filePath.ResolveVariables();
	_volumeDB.ResolveVariables();
	_startOffset.ResolveVariables();
	_playbackDuration.ResolveVariables();
}

MacroActionPlayAudioEdit::MacroActionPlayAudioEdit(
	QWidget *parent, std::shared_ptr<MacroActionPlayAudio> entryData)
	: QWidget(parent),
	  _filePath(new FileSelection(
		  FileSelection::Type::READ, this,
		  obs_module_text(
			  "AdvSceneSwitcher.action.playAudio.file.browse"))),
	  _volumeDB(new VariableDoubleSpinBox),
	  _monitorTypes(new QComboBox),
	  _tracksContainer(new QWidget),
	  _useStartOffset(new QCheckBox(this)),
	  _startOffset(new DurationSelection(this, true, 0.0)),
	  _useDuration(new QCheckBox(this)),
	  _playbackDuration(new DurationSelection(this, true, 0.0)),
	  _waitForCompletion(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.playAudio.wait")))
{
	_volumeDB->setMinimum(-100.0);
	_volumeDB->setMaximum(0.0);
	_volumeDB->setSuffix("dB");
	_volumeDB->setSpecialValueText("-inf");

	if (obs_audio_monitoring_available()) {
		PopulateMonitorTypeSelection(_monitorTypes);
	} else {
		_monitorTypes->addItem(obs_module_text(
			"AdvSceneSwitcher.action.playAudio.monitorUnavailable"));
		_monitorTypes->setEnabled(false);
	}

	for (int i = 0; i < 6; ++i) {
		_tracks[i] = new QCheckBox(QString::number(i + 1));
		QWidget::connect(_tracks[i], SIGNAL(stateChanged(int)), this,
				 SLOT(TrackChanged()));
	}

	QWidget::connect(_filePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(FilePathChanged(const QString &)));
	QWidget::connect(
		_volumeDB,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(VolumeDBChanged(const NumberVariable<double> &)));
	QWidget::connect(_monitorTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MonitorTypeChanged(int)));
	QWidget::connect(_useStartOffset, SIGNAL(stateChanged(int)), this,
			 SLOT(UseStartOffsetChanged(int)));
	QWidget::connect(_useDuration, SIGNAL(stateChanged(int)), this,
			 SLOT(UseDurationChanged(int)));
	QWidget::connect(_startOffset,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(StartOffsetChanged(const Duration &)));
	QWidget::connect(_playbackDuration,
			 SIGNAL(DurationChanged(const Duration &)), this,
			 SLOT(PlaybackDurationChanged(const Duration &)));
	QWidget::connect(_waitForCompletion, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitChanged(int)));

	auto tracksLayout = new QHBoxLayout;
	tracksLayout->setContentsMargins(0, 0, 0, 0);
	tracksLayout->addWidget(new QLabel(
		obs_module_text("AdvSceneSwitcher.action.playAudio.tracks")));
	for (auto *cb : _tracks) {
		tracksLayout->addWidget(cb);
	}
	tracksLayout->addStretch();
	_tracksContainer->setLayout(tracksLayout);

	auto volumeLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.action.playAudio.layout.volume"),
		     volumeLayout, {{"{{volumeDB}}", _volumeDB}});
	auto monitorLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.playAudio.layout.monitor"),
		monitorLayout, {{"{{monitorTypes}}", _monitorTypes}});
	auto startOffsetLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.playAudio.layout.startOffset"),
		startOffsetLayout,
		{{"{{useStartOffset}}", _useStartOffset},
		 {"{{startOffset}}", _startOffset}});
	auto playbackDurationLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.action.playAudio.layout.playbackDuration"),
		playbackDurationLayout,
		{{"{{useDuration}}", _useDuration},
		 {"{{playbackDuration}}", _playbackDuration}});

	auto mainLayout = new QVBoxLayout;
	mainLayout->addWidget(_filePath);
	mainLayout->addLayout(volumeLayout);
	mainLayout->addLayout(monitorLayout);
	mainLayout->addWidget(_tracksContainer);
	mainLayout->addLayout(startOffsetLayout);
	mainLayout->addLayout(playbackDurationLayout);
	mainLayout->addWidget(_waitForCompletion);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroActionPlayAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_filePath->SetPath(_entryData->_filePath);
	_volumeDB->SetValue(_entryData->_volumeDB);
	_monitorTypes->setCurrentIndex(
		static_cast<int>(_entryData->_monitorType));
	for (int i = 0; i < 6; ++i) {
		_tracks[i]->setChecked(_entryData->_audioMixers & (1u << i));
	}
	_tracksContainer->setVisible(_entryData->_monitorType !=
				     OBS_MONITORING_TYPE_MONITOR_ONLY);
	_useStartOffset->setChecked(_entryData->_useStartOffset);
	_startOffset->SetDuration(_entryData->_startOffset);
	_startOffset->setEnabled(_entryData->_useStartOffset);
	_useDuration->setChecked(_entryData->_useDuration);
	_playbackDuration->SetDuration(_entryData->_playbackDuration);
	_playbackDuration->setEnabled(_entryData->_useDuration);
	_waitForCompletion->setChecked(_entryData->_waitForCompletion);
}

void MacroActionPlayAudioEdit::FilePathChanged(const QString &path)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_filePath = path.toStdString();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionPlayAudioEdit::VolumeDBChanged(
	const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_volumeDB = value;
}

void MacroActionPlayAudioEdit::MonitorTypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_monitorType = static_cast<obs_monitoring_type>(value);
	_tracksContainer->setVisible(value != OBS_MONITORING_TYPE_MONITOR_ONLY);
}

void MacroActionPlayAudioEdit::TrackChanged()
{
	GUARD_LOADING_AND_LOCK();
	uint32_t mixers = 0;
	for (int i = 0; i < 6; ++i) {
		if (_tracks[i]->isChecked()) {
			mixers |= (1u << i);
		}
	}
	_entryData->_audioMixers = mixers;
}

void MacroActionPlayAudioEdit::UseStartOffsetChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useStartOffset = value;
	_startOffset->setEnabled(_entryData->_useStartOffset);
}

void MacroActionPlayAudioEdit::StartOffsetChanged(const Duration &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_startOffset = value;
}

void MacroActionPlayAudioEdit::UseDurationChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useDuration = value;
	_playbackDuration->setEnabled(_entryData->_useDuration);
}

void MacroActionPlayAudioEdit::PlaybackDurationChanged(const Duration &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_playbackDuration = value;
}

void MacroActionPlayAudioEdit::WaitChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_waitForCompletion = value;
}

} // namespace advss
