#include "headers/macro-action-audio.hpp"
#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

const std::string MacroActionAudio::id = "audio";

bool MacroActionAudio::_registered = MacroActionFactory::Register(
	MacroActionAudio::id,
	{MacroActionAudio::Create, MacroActionAudioEdit::Create,
	 "AdvSceneSwitcher.action.audio"});

const static std::map<AudioAction, std::string> actionTypes = {
	{AudioAction::MUTE, "AdvSceneSwitcher.action.audio.type.mute"},
	{AudioAction::UNMUTE, "AdvSceneSwitcher.action.audio.type.unmute"},
	{AudioAction::SOURCE_VOLUME,
	 "AdvSceneSwitcher.action.audio.type.sourceVolume"},
	{AudioAction::MASTER_VOLUME,
	 "AdvSceneSwitcher.action.audio.type.masterVolume"},
};

const static std::map<FadeType, std::string> fadeTypes = {
	{FadeType::DURATION,
	 "AdvSceneSwitcher.action.audio.fade.type.duration"},
	{FadeType::RATE, "AdvSceneSwitcher.action.audio.fade.type.rate"},
};

constexpr auto fadeInterval = std::chrono::milliseconds(100);
constexpr float minFade = 0.000001f;

void MacroActionAudio::SetFadeActive(bool value)
{
	if (_action == AudioAction::SOURCE_VOLUME) {
		switcher->activeAudioFades[GetWeakSourceName(_audioSource)]
			.active = value;
	} else {
		switcher->masterAudioFade.active = value;
	}
}

bool MacroActionAudio::FadeActive()
{
	bool active = true;
	if (_action == AudioAction::SOURCE_VOLUME) {
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

void MacroActionAudio::SetVolume(float vol)
{
	if (_action == AudioAction::SOURCE_VOLUME) {
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
	if (_action == AudioAction::SOURCE_VOLUME) {
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
	for (; step < nrSteps && !macro->GetStop(); ++step) {
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
	if (_action == AudioAction::SOURCE_VOLUME && !_audioSource) {
		return;
	}

	if (FadeActive()) {
		blog(LOG_WARNING,
		     "Audio fade for volume of %s already active! New fade request will be ignored!",
		     (_action == AudioAction::SOURCE_VOLUME)
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
	case AudioAction::MUTE:
		obs_source_set_muted(s, true);
		break;
	case AudioAction::UNMUTE:
		obs_source_set_muted(s, false);
		break;
	case AudioAction::SOURCE_VOLUME:
	case AudioAction::MASTER_VOLUME:
		if (_fade) {
			StartFade();
		} else {
			SetVolume((float)_volume / 100.0f);
		}
		break;
	default:
		break;
	}
	obs_source_release(s);
	return true;
}

void MacroActionAudio::LogAction()
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

bool MacroActionAudio::Save(obs_data_t *obj)
{
	MacroAction::Save(obj);
	_duration.Save(obj);
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(_audioSource).c_str());
	obs_data_set_int(obj, "action", static_cast<int>(_action));
	obs_data_set_int(obj, "volume", _volume);
	obs_data_set_double(obj, "rate", _rate);
	obs_data_set_bool(obj, "fade", _fade);
	obs_data_set_int(obj, "fadeType", static_cast<int>(_fadeType));
	obs_data_set_bool(obj, "wait", _wait);
	return true;
}

bool MacroActionAudio::Load(obs_data_t *obj)
{
	MacroAction::Load(obj);
	_duration.Load(obj);
	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	_audioSource = GetWeakSourceByName(audioSourceName);
	_action = static_cast<AudioAction>(obs_data_get_int(obj, "action"));
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
	return true;
}

std::string MacroActionAudio::GetShortDesc()
{
	if (_audioSource) {
		return GetWeakSourceName(_audioSource);
	}
	return "";
}

static inline void populateActionSelection(QComboBox *list)
{
	for (auto entry : actionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
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
	  _volumePercent(new QSpinBox),
	  _fade(new QCheckBox),
	  _wait(new QCheckBox),
	  _duration(new DurationSelection(parent, false)),
	  _rate(new QDoubleSpinBox),
	  _fadeTypes(new QComboBox)
{
	_volumePercent->setMinimum(0);
	_volumePercent->setMaximum(2000);
	_volumePercent->setSuffix("%");

	_rate->setMinimum(0.01);
	_rate->setMaximum(999.);
	_rate->setSuffix("%");

	populateActionSelection(_actions);
	populateAudioSelection(_audioSources);
	populateFadeTypeSelection(_fadeTypes);

	QWidget::connect(_actions, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ActionChanged(int)));
	QWidget::connect(_audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
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
	QWidget::connect(_fadeTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(FadeTypeChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", _audioSources},
		{"{{actions}}", _actions},
		{"{{volume}}", _volumePercent},
		{"{{fade}}", _fade},
		{"{{duration}}", _duration},
		{"{{rate}}", _rate},
		{"{{wait}}", _wait},
		{"{{fadeTypes}}", _fadeTypes},
	};
	QHBoxLayout *entryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.audio.entry"),
		     entryLayout, widgetPlaceholders);
	_fadeLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.action.audio.fade.duration"),
		_fadeLayout, widgetPlaceholders);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addLayout(_fadeLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

bool hasVolumeControl(AudioAction action)
{
	return action == AudioAction::SOURCE_VOLUME ||
	       action == AudioAction::MASTER_VOLUME;
}

bool hasSourceControl(AudioAction action)
{
	return action != AudioAction::MASTER_VOLUME;
}

void MacroActionAudioEdit::SetWidgetVisibility()
{
	_volumePercent->setVisible(hasVolumeControl(_entryData->_action));
	_audioSources->setVisible(hasSourceControl(_entryData->_action));

	_fadeLayout->removeWidget(_fade);
	_fadeLayout->removeWidget(_fadeTypes);
	_fadeLayout->removeWidget(_duration);
	_fadeLayout->removeWidget(_rate);
	_fadeLayout->removeWidget(_wait);
	clearLayout(_fadeLayout);
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{fade}}", _fade},           {"{{duration}}", _duration},
		{"{{rate}}", _rate},           {"{{wait}}", _wait},
		{"{{fadeTypes}}", _fadeTypes},
	};
	if (_entryData->_fadeType == FadeType::DURATION) {
		placeWidgets(
			obs_module_text(
				"AdvSceneSwitcher.action.audio.fade.duration"),
			_fadeLayout, widgetPlaceholders);
	} else {
		placeWidgets(obs_module_text(
				     "AdvSceneSwitcher.action.audio.fade.rate"),
			     _fadeLayout, widgetPlaceholders);
	}

	_duration->setVisible(_entryData->_fadeType == FadeType::DURATION);
	_rate->setVisible(_entryData->_fadeType == FadeType::RATE);

	setLayoutVisible(_fadeLayout, hasVolumeControl(_entryData->_action));
	adjustSize();
}

void MacroActionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_audioSources->setCurrentText(
		GetWeakSourceName(_entryData->_audioSource).c_str());
	_actions->setCurrentIndex(static_cast<int>(_entryData->_action));
	_volumePercent->setValue(_entryData->_volume);
	_fade->setChecked(_entryData->_fade);
	_duration->SetDuration(_entryData->_duration);
	_rate->setValue(_entryData->_rate);
	_wait->setChecked(_entryData->_wait);
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

void MacroActionAudioEdit::ActionChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_action = static_cast<AudioAction>(value);
	SetWidgetVisibility();
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

void MacroActionAudioEdit::FadeTypeChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_fadeType = static_cast<FadeType>(value);
	SetWidgetVisibility();
}
