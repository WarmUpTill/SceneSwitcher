#include "macro-action-audio.hpp"
#include "audio-helpers.hpp"
#include "layout-helpers.hpp"
#include "macro-helpers.hpp"
#include "selection-helpers.hpp"

#include <chrono>

namespace advss {

constexpr int64_t nsPerMs = 1000000;
const std::string MacroActionAudio::id = "audio";

bool MacroActionAudio::_registered = MacroActionFactory::Register(
	MacroActionAudio::id,
	{MacroActionAudio::Create, MacroActionAudioEdit::Create,
	 "AdvSceneSwitcher.action.audio"});

static const std::vector<std::pair<MacroActionAudio::Action, std::string>>
	actionTypes = {
		{MacroActionAudio::Action::MUTE,
		 "AdvSceneSwitcher.action.audio.type.mute"},
		{MacroActionAudio::Action::UNMUTE,
		 "AdvSceneSwitcher.action.audio.type.unmute"},
		{MacroActionAudio::Action::TOGGLE_MUTE,
		 "AdvSceneSwitcher.action.audio.type.toggleMute"},
		{MacroActionAudio::Action::SOURCE_VOLUME,
		 "AdvSceneSwitcher.action.audio.type.sourceVolume"},
		{MacroActionAudio::Action::MASTER_VOLUME,
		 "AdvSceneSwitcher.action.audio.type.masterVolume"},
		{MacroActionAudio::Action::SYNC_OFFSET,
		 "AdvSceneSwitcher.action.audio.type.syncOffset"},
		{MacroActionAudio::Action::MONITOR,
		 "AdvSceneSwitcher.action.audio.type.monitor"},
		{MacroActionAudio::Action::BALANCE,
		 "AdvSceneSwitcher.action.audio.type.balance"},
		{MacroActionAudio::Action::ENABLE_ON_TRACK,
		 "AdvSceneSwitcher.action.audio.type.enableOnTrack"},
		{MacroActionAudio::Action::DISABLE_ON_TRACK,
		 "AdvSceneSwitcher.action.audio.type.disableOnTrack"},
};

static const std::map<MacroActionAudio::FadeType, std::string> fadeTypes = {
	{MacroActionAudio::FadeType::DURATION,
	 "AdvSceneSwitcher.action.audio.fade.type.duration"},
	{MacroActionAudio::FadeType::RATE,
	 "AdvSceneSwitcher.action.audio.fade.type.rate"},
};

namespace {
struct FadeInfo {
	std::atomic_bool active = {false};
	std::atomic_int id = {0};
};
} // namespace

static FadeInfo masterAudioFade;
static std::unordered_map<std::string, FadeInfo> audioFades;

constexpr auto fadeInterval = std::chrono::milliseconds(100);
constexpr float minFade = 0.000001f;

// For backwards compatibility
#if LIBOBS_API_VER >= MAKE_SEMANTIC_VERSION(29, 0, 0)
static float get_master_volume()
{
	return 1.f;
}

static void set_master_volume(float)
{
	return;
}
#else
auto get_master_volume = obs_get_master_volume;
auto set_master_volume = obs_set_master_volume;
#endif

void MacroActionAudio::SetFadeActive(bool value) const
{
	if (_action == Action::SOURCE_VOLUME) {
		audioFades[_audioSource.ToString()].active = value;
	} else {
		masterAudioFade.active = value;
	}
}

bool MacroActionAudio::FadeActive() const
{
	bool active = true;
	if (_action == Action::SOURCE_VOLUME) {
		auto it = audioFades.find(_audioSource.ToString());
		if (it == audioFades.end()) {
			return false;
		}
		active = it->second.active;
	} else {
		active = masterAudioFade.active;
	}

	return active;
}

std::atomic_int *MacroActionAudio::GetFadeIdPtr() const
{

	if (_action == Action::SOURCE_VOLUME) {
		auto it = audioFades.find(_audioSource.ToString());
		if (it == audioFades.end()) {
			return nullptr;
		}
		return &it->second.id;
	}
	return &masterAudioFade.id;
}

float MacroActionAudio::GetVolume() const
{
	return _useDb ? DecibelToPercent(_volumeDB) : (float)_volume / 100.0f;
}

void MacroActionAudio::SetVolume(float vol) const
{
	if (_action == Action::SOURCE_VOLUME) {
		auto s = obs_weak_source_get_source(_audioSource.GetSource());
		obs_source_set_volume(s, vol);
		obs_source_release(s);
	} else {
		set_master_volume(vol);
	}
}

float MacroActionAudio::GetCurrentVolume() const
{
	float curVol;
	if (_action == Action::SOURCE_VOLUME) {
		auto s = obs_weak_source_get_source(_audioSource.GetSource());
		if (!s) {
			return 0.;
		}
		curVol = obs_source_get_volume(s);
		obs_source_release(s);
	} else {
		curVol = get_master_volume();
	}
	return curVol;
}

void MacroActionAudio::FadeVolume() const
{
	float vol = GetVolume();
	float curVol = GetCurrentVolume();
	bool volIncrease = curVol <= vol;
	float volDiff = (volIncrease) ? vol - curVol : curVol - vol;
	int nrSteps = 0;
	float volStep = 0.;
	if (_fadeType == FadeType::DURATION) {
		nrSteps = _duration.Milliseconds() / fadeInterval.count();
		volStep = volDiff / nrSteps;
	} else {
		volStep = _rate / 1000.0f;
		nrSteps = volDiff / volStep;
	}

	if (volStep < minFade || nrSteps <= 1) {
		SetVolume(vol);
		SetFadeActive(false);
		return;
	}

	auto macro = GetMacro();
	int step = 0;
	auto fadeId = GetFadeIdPtr();
	int expectedFadeId = ++(*fadeId);
	for (; step < nrSteps && !MacroIsStopped(macro) &&
	       expectedFadeId == *fadeId;
	     ++step) {
		curVol = (volIncrease) ? curVol + volStep : curVol - volStep;
		SetVolume(curVol);
		std::this_thread::sleep_for(fadeInterval);
	}

	// As a final step set desired volume once again in case floating-point
	// precision errors compounded to a noticeable error
	if (step == nrSteps) {
		SetVolume(vol);
	}

	SetFadeActive(false);
}

void MacroActionAudio::StartFade() const
{
	if (_action == Action::SOURCE_VOLUME && !_audioSource.GetSource()) {
		return;
	}

	if (FadeActive() && !_abortActiveFade) {
		blog(LOG_WARNING,
		     "Audio fade for volume of %s already active! New fade request will be ignored!",
		     (_action == Action::SOURCE_VOLUME)
			     ? _audioSource.ToString(true).c_str()
			     : "master volume");
		return;
	}
	SetFadeActive(true);

	if (_wait) {
		FadeVolume();
	} else {
		AddMacroHelperThread(GetMacro(),
				     std::thread(&MacroActionAudio::FadeVolume,
						 this));
	}
}

static void setMixerEnable(obs_source_t *source, const int mixerIdx,
			   const bool enable)
{
	if (mixerIdx < 0) {
		blog(LOG_INFO, "refusing to %s mixer id %d for %s",
		     enable ? "enable" : "disable", mixerIdx,
		     obs_source_get_name(source));
		return;
	}

	uint32_t mixers = obs_source_get_audio_mixers(source);
	uint32_t new_mixers = mixers;

	if (enable) {
		new_mixers |= (1 << mixerIdx);
	} else {
		new_mixers &= ~(1 << mixerIdx);
	}

	obs_source_set_audio_mixers(source, new_mixers);
}

static void setSyncOffsetHelper(obs_source_t *source, int64_t value)
{
	// Workaround for this issue:
	// https://github.com/obsproject/obs-studio/issues/7912
	using namespace std::chrono_literals;
	obs_source_set_sync_offset(source, value - 2);
	std::this_thread::sleep_for(10ms);
	obs_source_set_sync_offset(source, value - 1);
	std::this_thread::sleep_for(10ms);
	obs_source_set_sync_offset(source, value);
}

bool MacroActionAudio::PerformAction()
{
	auto s = obs_weak_source_get_source(_audioSource.GetSource());
	switch (_action) {
	case Action::MUTE:
		obs_source_set_muted(s, true);
		break;
	case Action::UNMUTE:
		obs_source_set_muted(s, false);
		break;
	case Action::TOGGLE_MUTE:
		obs_source_set_muted(s, !obs_source_muted(s));
		break;
	case Action::SOURCE_VOLUME:
	case Action::MASTER_VOLUME:
		if (_fade) {
			StartFade();
		} else {
			SetVolume(GetVolume());
		}
		break;
	case Action::SYNC_OFFSET:
		setSyncOffsetHelper(s, _syncOffset * nsPerMs);
		break;
	case Action::MONITOR:
		obs_source_set_monitoring_type(s, _monitorType);
		break;
	case Action::BALANCE:
		obs_source_set_balance_value(s, _balance);
		break;
	case Action::ENABLE_ON_TRACK:
		setMixerEnable(s, _track - 1, true);
		break;
	case Action::DISABLE_ON_TRACK:
		setMixerEnable(s, _track - 1, false);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionAudio::LogAction() const
{
	auto it = std::find_if(
		actionTypes.begin(), actionTypes.end(),
		[this](const std::pair<MacroActionAudio::Action, std::string>
			       &item) { return item.first == _action; });
	if (it != actionTypes.end()) {
		auto &[_, action] = *it;
		ablog(LOG_INFO,
		      "performed action \"%s\" for source \"%s\" with volume %f with fade %d %f",
		      action.c_str(), _audioSource.ToString(true).c_str(),
		      GetVolume(), _fade, _duration.Seconds());
	} else {
		blog(LOG_WARNING, "ignored unknown audio action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionAudio::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_duration.Save(obj);
	_audioSource.Save(obj, "audioSource");
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "monitor", _monitorType);
	_syncOffset.Save(obj, "syncOffset");
	_balance.Save(obj, "balance");
	_track.Save(obj, "track");
	_volume.Save(obj, "volume");
	_rate.Save(obj, "rate");
	obs_data_set_bool(obj, "fade", _fade);
	obs_data_set_int(obj, "fadeType", static_cast<int>(_fadeType));
	obs_data_set_bool(obj, "wait", _wait);
	obs_data_set_bool(obj, "abortActiveFade", _abortActiveFade);
	obs_data_set_bool(obj, "useDb", _useDb);
	_volumeDB.Save(obj, "volumeDB");
	obs_data_set_int(obj, "version", 3);
	return true;
}

bool MacroActionAudio::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration.Load(obj);
	_audioSource.Load(obj, "audioSource");
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_monitorType = static_cast<obs_monitoring_type>(
		obs_data_get_int(obj, "monitor"));
	if (!obs_data_has_user_value(obj, "version")) {
		_syncOffset = obs_data_get_int(obj, "syncOffset");
		_balance = obs_data_get_double(obj, "balance");
		_volume = obs_data_get_int(obj, "volume");
		_rate = obs_data_get_double(obj, "rate");
	} else {
		_syncOffset.Load(obj, "syncOffset");
		_balance.Load(obj, "balance");
		_volume.Load(obj, "volume");
		_rate.Load(obj, "rate");
	}
	_fade = obs_data_get_bool(obj, "fade");
	if (obs_data_has_user_value(obj, "wait")) {
		_wait = obs_data_get_bool(obj, "wait");
	} else {
		_wait = false;
	}
	if (obs_data_has_user_value(obj, "fadeType")) {
		_fadeType = static_cast<FadeType>(
			obs_data_get_int(obj, "fadeType"));
	} else {
		_fadeType = FadeType::DURATION;
	}
	if (obs_data_has_user_value(obj, "abortActiveFade")) {
		_abortActiveFade = obs_data_get_bool(obj, "abortActiveFade");
	} else {
		_abortActiveFade = false;
	}

	if (obs_data_get_int(obj, "version") < 2) {
		_useDb = false;
		_volumeDB = 0.0;
	} else {
		_useDb = obs_data_get_bool(obj, "useDb");
		_volumeDB.Load(obj, "volumeDB");
	}
	if (obs_data_get_int(obj, "version") < 3) {
		_track = 1;
	} else {
		_track.Load(obj, "track");
	}

	return true;
}

std::string MacroActionAudio::GetShortDesc() const
{
	return _audioSource.ToString();
}

std::shared_ptr<MacroAction> MacroActionAudio::Create(Macro *m)
{
	return std::make_shared<MacroActionAudio>(m);
}

std::shared_ptr<MacroAction> MacroActionAudio::Copy() const
{
	return std::make_shared<MacroActionAudio>(*this);
}

void MacroActionAudio::ResolveVariablesToFixedValues()
{
	_audioSource.ResolveVariables();
	_syncOffset.ResolveVariables();
	_balance.ResolveVariables();
	_volume.ResolveVariables();
	_volumeDB.ResolveVariables();
	_duration.ResolveVariables();
	_rate.ResolveVariables();
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto &[action, name] : actionTypes) {
		if (action == MacroActionAudio::Action::MONITOR &&
		    !obs_audio_monitoring_available()) {
			continue;
		}

		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(action));
	}
}

static inline void populateFadeTypeSelection(QComboBox *list)
{
	for (const auto &entry : fadeTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static QStringList getAudioSourcesList()
{
	auto sources = GetAudioSourceNames();
	sources.sort();
	return sources;
}

MacroActionAudioEdit::MacroActionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroActionAudio> entryData)
	: QWidget(parent),
	  _sources(new SourceSelectionWidget(this, getAudioSourcesList, true)),
	  _actions(new QComboBox),
	  _fadeTypes(new QComboBox),
	  _syncOffset(new VariableSpinBox),
	  _monitorTypes(new QComboBox),
	  _balance(new SliderSpinBox(
		  0., 1., "",
		  obs_module_text(
			  "AdvSceneSwitcher.action.audio.balance.description"))),
	  _track(new VariableSpinBox),
	  _volumePercent(new VariableSpinBox),
	  _volumeDB(new VariableDoubleSpinBox),
	  _percentDBToggle(new QPushButton),
	  _fade(new QCheckBox),
	  _duration(new DurationSelection(parent, false)),
	  _rate(new VariableDoubleSpinBox),
	  _wait(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.audio.fade.wait"))),
	  _abortActiveFade(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.action.audio.fade.abort"))),
	  _fadeTypeLayout(new QHBoxLayout),
	  _fadeOptionsLayout(new QVBoxLayout)
{
	_syncOffset->setMinimum(-950);
	_syncOffset->setMaximum(20000);
	_syncOffset->setSuffix("ms");

	_track->setMinimum(1);
	_track->setMaximum(32);

	_volumePercent->setMinimum(0);
	_volumePercent->setMaximum(2000);
	_volumePercent->setSuffix("%");

	_volumeDB->setMinimum(-100);
	_volumeDB->setMaximum(0);
	_volumeDB->setSuffix("dB");
	_volumeDB->setSpecialValueText("-inf");

	_rate->setMinimum(0.01);
	_rate->setMaximum(999.);
	_rate->setSuffix("%");

	populateActionSelection(_actions);
	populateFadeTypeSelection(_fadeTypes);
	PopulateMonitorTypeSelection(_monitorTypes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(
		_syncOffset,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(SyncOffsetChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_balance,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this, SLOT(BalanceChanged(const NumberVariable<double> &)));
	QWidget::connect(_monitorTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MonitorTypeChanged(int)));
	QWidget::connect(
		_track,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(TrackChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_volumePercent,
		SIGNAL(NumberVariableChanged(const NumberVariable<int> &)),
		this, SLOT(VolumeChanged(const NumberVariable<int> &)));
	QWidget::connect(
		_volumeDB,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(VolumeDBChanged(const NumberVariable<double> &)));
	QWidget::connect(_percentDBToggle, SIGNAL(clicked()), this,
			 SLOT(PercentDBClicked()));
	QWidget::connect(_fade, SIGNAL(stateChanged(int)), this,
			 SLOT(FadeChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(
		_rate,
		SIGNAL(NumberVariableChanged(const NumberVariable<double> &)),
		this, SLOT(RateChanged(const NumberVariable<double> &)));
	QWidget::connect(_wait, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitChanged(int)));
	QWidget::connect(_abortActiveFade, SIGNAL(stateChanged(int)), this,
			 SLOT(AbortActiveFadeChanged(int)));
	QWidget::connect(_fadeTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FadeTypeChanged(int)));

	const std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", _sources},
		{"{{actions}}", _actions},
		{"{{syncOffset}}", _syncOffset},
		{"{{monitorTypes}}", _monitorTypes},
		{"{{balance}}", _balance},
		{"{{track}}", _track},
		{"{{volume}}", _volumePercent},
		{"{{volumeDB}}", _volumeDB},
		{"{{percentDBToggle}}", _percentDBToggle},
		{"{{fade}}", _fade},
		{"{{duration}}", _duration},
		{"{{rate}}", _rate},
		{"{{wait}}", _wait},
		{"{{abortActiveFade}}", _abortActiveFade},
		{"{{fadeTypes}}", _fadeTypes},
	};
	QHBoxLayout *entryLayout = new QHBoxLayout;
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.action.audio.entry"),
		     entryLayout, widgetPlaceholders);
	_fadeTypeLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.action.audio.fade.duration"),
		_fadeTypeLayout, widgetPlaceholders);

	_fadeOptionsLayout->addLayout(_fadeTypeLayout);
	_fadeOptionsLayout->addWidget(_abortActiveFade);
	_fadeOptionsLayout->addWidget(_wait);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_balance);
	mainLayout->addLayout(_fadeOptionsLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

static bool hasVolumeControl(MacroActionAudio::Action action)
{
	return action == MacroActionAudio::Action::SOURCE_VOLUME ||
	       action == MacroActionAudio::Action::MASTER_VOLUME;
}

void MacroActionAudioEdit::SetWidgetVisibility()
{
	_volumePercent->setVisible(hasVolumeControl(_entryData->_action) &&
				   !_entryData->_useDb);
	_volumeDB->setVisible(hasVolumeControl(_entryData->_action) &&
			      _entryData->_useDb);
	_percentDBToggle->setText(_entryData->_useDb ? "dB" : "%");
	_percentDBToggle->setVisible(hasVolumeControl(_entryData->_action));
	_sources->setVisible(_entryData->_action !=
			     MacroActionAudio::Action::MASTER_VOLUME);
	_syncOffset->setVisible(_entryData->_action ==
				MacroActionAudio::Action::SYNC_OFFSET);
	_monitorTypes->setVisible(_entryData->_action ==
				  MacroActionAudio::Action::MONITOR);
	_balance->setVisible(_entryData->_action ==
			     MacroActionAudio::Action::BALANCE);
	_track->setVisible(_entryData->_action ==
				   MacroActionAudio::Action::ENABLE_ON_TRACK ||
			   _entryData->_action ==
				   MacroActionAudio::Action::DISABLE_ON_TRACK);

	_fadeTypeLayout->removeWidget(_fade);
	_fadeTypeLayout->removeWidget(_fadeTypes);
	_fadeTypeLayout->removeWidget(_duration);
	_fadeTypeLayout->removeWidget(_rate);
	ClearLayout(_fadeTypeLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fade}}", _fade},
		{"{{duration}}", _duration},
		{"{{rate}}", _rate},
		{"{{fadeTypes}}", _fadeTypes},
	};
	if (_entryData->_fadeType == MacroActionAudio::FadeType::DURATION) {
		PlaceWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.audio.fade.duration"),
			_fadeTypeLayout, widgetPlaceholders);
	} else {
		PlaceWidgets(obs_module_text(
				     "AdvSceneSwitcher.action.audio.fade.rate"),
			     _fadeTypeLayout, widgetPlaceholders);
	}

	_duration->setVisible(_entryData->_fadeType ==
			      MacroActionAudio::FadeType::DURATION);
	_rate->setVisible(_entryData->_fadeType ==
			  MacroActionAudio::FadeType::RATE);

	SetLayoutVisible(_fadeTypeLayout,
			 hasVolumeControl(_entryData->_action));
	SetLayoutVisible(_fadeOptionsLayout,
			 hasVolumeControl(_entryData->_action));
	_abortActiveFade->setVisible(hasVolumeControl(_entryData->_action) &&
				     _entryData->_fade);
	_wait->setVisible(hasVolumeControl(_entryData->_action) &&
			  _entryData->_fade);
	_duration->setEnabled(_entryData->_fade);
	_rate->setEnabled(_entryData->_fade);
	_fadeTypes->setEnabled(_entryData->_fade);

	// TODO: Remove this in a future version:
	if (_entryData->_action != MacroActionAudio::Action::MASTER_VOLUME &&
	    obs_get_version() >= MAKE_SEMANTIC_VERSION(29, 0, 0)) {
		_actions->removeItem(_actions->findText(obs_module_text(
			"AdvSceneSwitcher.action.audio.type.masterVolume")));
	}

	updateGeometry();
	adjustSize();
}

void MacroActionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_sources->SetSource(_entryData->_audioSource);
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_syncOffset->SetValue(_entryData->_syncOffset);
	_monitorTypes->setCurrentIndex(_entryData->_monitorType);
	_balance->SetDoubleValue(_entryData->_balance);
	_track->SetValue(_entryData->_track);
	_volumePercent->SetValue(_entryData->_volume);
	_volumeDB->SetValue(_entryData->_volumeDB);
	_fade->setChecked(_entryData->_fade);
	_duration->SetDuration(_entryData->_duration);
	_rate->SetValue(_entryData->_rate);
	_wait->setChecked(_entryData->_wait);
	_abortActiveFade->setChecked(_entryData->_abortActiveFade);
	_fadeTypes->setCurrentIndex(static_cast<int>(_entryData->_fadeType));
	SetWidgetVisibility();
}

void MacroActionAudioEdit::SourceChanged(const SourceSelection &source)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_audioSource = source;
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionAudioEdit::ActionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_action = static_cast<MacroActionAudio::Action>(
		_actions->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionAudioEdit::SyncOffsetChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_syncOffset = value;
}

void MacroActionAudioEdit::MonitorTypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_monitorType = static_cast<obs_monitoring_type>(value);
}

void MacroActionAudioEdit::BalanceChanged(const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_balance = value;
}

void MacroActionAudioEdit::TrackChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_track = value;
}

void MacroActionAudioEdit::VolumeDBChanged(const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_volumeDB = value;
}

void MacroActionAudioEdit::PercentDBClicked()
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_useDb = !_entryData->_useDb;
	SetWidgetVisibility();
}

void MacroActionAudioEdit::VolumeChanged(const NumberVariable<int> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_volume = value;
}

void MacroActionAudioEdit::FadeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fade = value;
	SetWidgetVisibility();
}

void MacroActionAudioEdit::DurationChanged(const Duration &dur)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_duration = dur;
}

void MacroActionAudioEdit::RateChanged(const NumberVariable<double> &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_rate = value;
}

void MacroActionAudioEdit::WaitChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_wait = value;
}

void MacroActionAudioEdit::AbortActiveFadeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_abortActiveFade = value;
}

void MacroActionAudioEdit::FadeTypeChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_fadeType = static_cast<MacroActionAudio::FadeType>(value);
	SetWidgetVisibility();
}

} // namespace advss
