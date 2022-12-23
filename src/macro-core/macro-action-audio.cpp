#include "macro-action-audio.hpp"
#include "advanced-scene-switcher.hpp"
#include "utility.hpp"

constexpr int64_t nsPerMs = 1000000;
const std::string MacroActionAudio::id = "audio";

bool MacroActionAudio::_registered = MacroActionFactory::Register(
	MacroActionAudio::id,
	{MacroActionAudio::Create, MacroActionAudioEdit::Create,
	 "AdvSceneSwitcher.action.audio"});

const static std::map<MacroActionAudio::Action, std::string> actionTypes = {
	{MacroActionAudio::Action::MUTE,
	 "AdvSceneSwitcher.action.audio.type.mute"},
	{MacroActionAudio::Action::UNMUTE,
	 "AdvSceneSwitcher.action.audio.type.unmute"},
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
};

const static std::map<MacroActionAudio::FadeType, std::string> fadeTypes = {
	{MacroActionAudio::FadeType::DURATION,
	 "AdvSceneSwitcher.action.audio.fade.type.duration"},
	{MacroActionAudio::FadeType::RATE,
	 "AdvSceneSwitcher.action.audio.fade.type.rate"},
};

constexpr auto fadeInterval = std::chrono::milliseconds(100);
constexpr float minFade = 0.000001f;

void MacroActionAudio::SetFadeActive(bool value)
{
	if (_action == Action::SOURCE_VOLUME) {
		switcher->activeAudioFades[GetWeakSourceName(_audioSource)]
			.active = value;
	} else {
		switcher->masterAudioFade.active = value;
	}
}

bool MacroActionAudio::FadeActive()
{
	bool active = true;
	if (_action == Action::SOURCE_VOLUME) {
		auto it = switcher->activeAudioFades.find(
			GetWeakSourceName(_audioSource));
		if (it == switcher->activeAudioFades.end()) {
			return false;
		}
		active = it->second.active;
	} else {
		active = switcher->masterAudioFade.active;
	}

	return active;
}

std::atomic_int *MacroActionAudio::GetFadeIdPtr()
{

	if (_action == Action::SOURCE_VOLUME) {
		auto it = switcher->activeAudioFades.find(
			GetWeakSourceName(_audioSource));
		if (it == switcher->activeAudioFades.end()) {
			return nullptr;
		}
		return &it->second.id;
	}
	return &switcher->masterAudioFade.id;
}

void MacroActionAudio::SetVolume(float vol)
{
	if (_action == Action::SOURCE_VOLUME) {
		auto s = obs_weak_source_get_source(_audioSource);
		obs_source_set_volume(s, vol);
		obs_source_release(s);
	} else {
		obs_set_master_volume(vol);
	}
}

float MacroActionAudio::GetVolume()
{
	float curVol;
	if (_action == Action::SOURCE_VOLUME) {
		auto s = obs_weak_source_get_source(_audioSource);
		if (!s) {
			return 0.;
		}
		curVol = obs_source_get_volume(s);
		obs_source_release(s);
	} else {
		curVol = obs_get_master_volume();
	}
	return curVol;
}

void MacroActionAudio::FadeVolume()
{
	float vol = (float)_volume / 100.0f;
	float curVol = GetVolume();
	bool volIncrease = curVol <= vol;
	float volDiff = (volIncrease) ? vol - curVol : curVol - vol;
	int nrSteps = 0;
	float volStep = 0.;
	if (_fadeType == FadeType::DURATION) {
		nrSteps = _duration.seconds * 1000 / fadeInterval.count();
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
	for (; step < nrSteps && !macro->GetStop() && expectedFadeId == *fadeId;
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

void MacroActionAudio::StartFade()
{
	if (_action == Action::SOURCE_VOLUME && !_audioSource) {
		return;
	}

	if (FadeActive() && !_abortActiveFade) {
		blog(LOG_WARNING,
		     "Audio fade for volume of %s already active! New fade request will be ignored!",
		     (_action == Action::SOURCE_VOLUME)
			     ? GetWeakSourceName(_audioSource).c_str()
			     : "master volume");
		return;
	}
	SetFadeActive(true);

	if (_wait) {
		FadeVolume();
	} else {
		GetMacro()->AddHelperThread(
			std::thread(&MacroActionAudio::FadeVolume, this));
	}
}

bool MacroActionAudio::PerformAction()
{
	auto s = obs_weak_source_get_source(_audioSource);
	switch (_action) {
	case Action::MUTE:
		obs_source_set_muted(s, true);
		break;
	case Action::UNMUTE:
		obs_source_set_muted(s, false);
		break;
	case Action::SOURCE_VOLUME:
	case Action::MASTER_VOLUME:
		if (_fade) {
			StartFade();
		} else {
			SetVolume((float)_volume / 100.0f);
		}
		break;
	case Action::SYNC_OFFSET:
		obs_source_set_sync_offset(s, _syncOffset * nsPerMs);
		break;
	case Action::MONITOR:
		obs_source_set_monitoring_type(s, _monitorType);
		break;
	case Action::BALANCE:
		obs_source_set_balance_value(s, _balance);
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionAudio::LogAction() const
{
	auto it = actionTypes.find(_action);
	if (it != actionTypes.end()) {
		vblog(LOG_INFO,
		      "performed action \"%s\" for source \"%s\" with volume %d with fade %d %f",
		      it->second.c_str(),
		      GetWeakSourceName(_audioSource).c_str(), _volume, _fade,
		      _duration.seconds);
	} else {
		blog(LOG_WARNING, "ignored unknown audio action %d",
		     static_cast<int>(_action));
	}
}

bool MacroActionAudio::Save(obs_data_t *obj) const
{
	MacroAction::Save(obj);
	_duration.Save(obj);
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(_audioSource).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "syncOffset", _syncOffset);
	obs_data_set_int(obj, "monitor", _monitorType);
	obs_data_set_double(obj, "balance", _balance);
	obs_data_set_int(obj, "volume", _volume);
	obs_data_set_double(obj, "rate", _rate);
	obs_data_set_bool(obj, "fade", _fade);
	obs_data_set_int(obj, "fadeType", static_cast<int>(_fadeType));
	obs_data_set_bool(obj, "wait", _wait);
	obs_data_set_bool(obj, "abortActiveFade", _abortActiveFade);
	return true;
}

bool MacroActionAudio::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration.Load(obj);
	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	_audioSource = GetWeakSourceByName(audioSourceName);
	_action = static_cast<Action>(obs_data_get_int(obj, "action"));
	_syncOffset = obs_data_get_int(obj, "syncOffset");
	_monitorType = static_cast<obs_monitoring_type>(
		obs_data_get_int(obj, "monitor"));
	_balance = obs_data_get_double(obj, "balance");
	_volume = obs_data_get_int(obj, "volume");
	_rate = obs_data_get_double(obj, "rate");
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
	return true;
}

std::string MacroActionAudio::GetShortDesc() const
{
	if (_audioSource) {
		return GetWeakSourceName(_audioSource);
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (const auto [action, name] : actionTypes) {
		if (action == MacroActionAudio::Action::MONITOR) {
			if (obs_audio_monitoring_available()) {
				list->addItem(obs_module_text(name.c_str()),
					      static_cast<int>(action));
			}
		} else {
			list->addItem(obs_module_text(name.c_str()),
				      static_cast<int>(action));
		}
	}
}

static inline void populateFadeTypeSelection(QComboBox *list)
{
	for (auto entry : fadeTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroActionAudioEdit::MacroActionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroActionAudio> entryData)
	: QWidget(parent),
	  _audioSources(new QComboBox),
	  _actions(new QComboBox),
	  _fadeTypes(new QComboBox),
	  _syncOffset(new QSpinBox),
	  _monitorTypes(new QComboBox),
	  _balance(new SliderSpinBox(
		  0., 1., "",
		  obs_module_text(
			  "AdvSceneSwitcher.action.audio.balance.description"))),
	  _volumePercent(new QSpinBox),
	  _fade(new QCheckBox),
	  _duration(new DurationSelection(parent, false)),
	  _rate(new QDoubleSpinBox),
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

	_volumePercent->setMinimum(0);
	_volumePercent->setMaximum(2000);
	_volumePercent->setSuffix("%");

	_rate->setMinimum(0.01);
	_rate->setMaximum(999.);
	_rate->setSuffix("%");

	populateActionSelection(_actions);
	populateAudioSelection(_audioSources);
	populateFadeTypeSelection(_fadeTypes);
	populateMonitorTypeSelection(_monitorTypes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_syncOffset, SIGNAL(valueChanged(int)), this,
			 SLOT(SyncOffsetChanged(int)));
	QWidget::connect(_balance, SIGNAL(DoubleValueChanged(double)), this,
			 SLOT(BalanceChanged(double)));
	QWidget::connect(_monitorTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(MonitorTypeChanged(int)));
	QWidget::connect(_volumePercent, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeChanged(int)));
	QWidget::connect(_fade, SIGNAL(stateChanged(int)), this,
			 SLOT(FadeChanged(int)));
	QWidget::connect(_duration, SIGNAL(DurationChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(_rate, SIGNAL(valueChanged(double)), this,
			 SLOT(RateChanged(double)));
	QWidget::connect(_wait, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitChanged(int)));
	QWidget::connect(_abortActiveFade, SIGNAL(stateChanged(int)), this,
			 SLOT(AbortActiveFadeChanged(int)));
	QWidget::connect(_fadeTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FadeTypeChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", _audioSources},
		{"{{actions}}", _actions},
		{"{{syncOffset}}", _syncOffset},
		{"{{monitorTypes}}", _monitorTypes},
		{"{{balance}}", _balance},
		{"{{volume}}", _volumePercent},
		{"{{fade}}", _fade},
		{"{{duration}}", _duration},
		{"{{rate}}", _rate},
		{"{{wait}}", _wait},
		{"{{abortActiveFade}}", _abortActiveFade},
		{"{{fadeTypes}}", _fadeTypes},
	};
	QHBoxLayout *entryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.audio.entry"),
		     entryLayout, widgetPlaceholders);
	_fadeTypeLayout = new QHBoxLayout;
	placeWidgets(
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

bool hasVolumeControl(MacroActionAudio::Action action)
{
	return action == MacroActionAudio::Action::SOURCE_VOLUME ||
	       action == MacroActionAudio::Action::MASTER_VOLUME;
}

void MacroActionAudioEdit::SetWidgetVisibility()
{
	_volumePercent->setVisible(hasVolumeControl(_entryData->_action));
	_audioSources->setVisible(_entryData->_action !=
				  MacroActionAudio::Action::MASTER_VOLUME);
	_syncOffset->setVisible(_entryData->_action ==
				MacroActionAudio::Action::SYNC_OFFSET);
	_monitorTypes->setVisible(_entryData->_action ==
				  MacroActionAudio::Action::MONITOR);
	_balance->setVisible(_entryData->_action ==
			     MacroActionAudio::Action::BALANCE);

	_fadeTypes->setDisabled(!_entryData->_fade);
	_wait->setDisabled(!_entryData->_fade);
	_abortActiveFade->setDisabled(!_entryData->_fade);
	_duration->setDisabled(!_entryData->_fade);
	_rate->setDisabled(!_entryData->_fade);

	_fadeTypeLayout->removeWidget(_fade);
	_fadeTypeLayout->removeWidget(_fadeTypes);
	_fadeTypeLayout->removeWidget(_duration);
	_fadeTypeLayout->removeWidget(_rate);
	clearLayout(_fadeTypeLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fade}}", _fade},
		{"{{duration}}", _duration},
		{"{{rate}}", _rate},
		{"{{fadeTypes}}", _fadeTypes},
	};
	if (_entryData->_fadeType == MacroActionAudio::FadeType::DURATION) {
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.audio.fade.duration"),
			_fadeTypeLayout, widgetPlaceholders);
	} else {
		placeWidgets(obs_module_text(
				     "AdvSceneSwitcher.action.audio.fade.rate"),
			     _fadeTypeLayout, widgetPlaceholders);
	}

	_duration->setVisible(_entryData->_fadeType ==
			      MacroActionAudio::FadeType::DURATION);
	_rate->setVisible(_entryData->_fadeType ==
			  MacroActionAudio::FadeType::RATE);

	setLayoutVisible(_fadeTypeLayout,
			 hasVolumeControl(_entryData->_action));
	setLayoutVisible(_fadeOptionsLayout,
			 hasVolumeControl(_entryData->_action));
	adjustSize();
}

void MacroActionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_audioSources->setCurrentText(
		GetWeakSourceName(_entryData->_audioSource).c_str());
	_actions->setCurrentIndex(
		_actions->findData(static_cast<int>(_entryData->_action)));
	_syncOffset->setValue(_entryData->_syncOffset);
	_monitorTypes->setCurrentIndex(_entryData->_monitorType);
	_balance->SetDoubleValue(_entryData->_balance);
	_volumePercent->setValue(_entryData->_volume);
	_fade->setChecked(_entryData->_fade);
	_duration->SetDuration(_entryData->_duration);
	_rate->setValue(_entryData->_rate);
	_wait->setChecked(_entryData->_wait);
	_abortActiveFade->setChecked(_entryData->_abortActiveFade);
	_fadeTypes->setCurrentIndex(static_cast<int>(_entryData->_fadeType));
	SetWidgetVisibility();
}

void MacroActionAudioEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_audioSource = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroActionAudioEdit::ActionChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<MacroActionAudio::Action>(
		_actions->itemData(idx).toInt());
	SetWidgetVisibility();
}

void MacroActionAudioEdit::SyncOffsetChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_syncOffset = value;
}

void MacroActionAudioEdit::MonitorTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_monitorType = static_cast<obs_monitoring_type>(value);
}

void MacroActionAudioEdit::BalanceChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_balance = value;
}

void MacroActionAudioEdit::VolumeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_volume = value;
}

void MacroActionAudioEdit::FadeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_fade = value;
	SetWidgetVisibility();
}

void MacroActionAudioEdit::DurationChanged(double seconds)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_duration.seconds = seconds;
}

void MacroActionAudioEdit::RateChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_rate = value;
}

void MacroActionAudioEdit::WaitChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_wait = value;
}

void MacroActionAudioEdit::AbortActiveFadeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_abortActiveFade = value;
}

void MacroActionAudioEdit::FadeTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_fadeType = static_cast<MacroActionAudio::FadeType>(value);
	SetWidgetVisibility();
}
