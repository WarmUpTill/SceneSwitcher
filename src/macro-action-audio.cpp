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

constexpr auto fadeInterval = std::chrono::milliseconds(100);
constexpr float minFade = 0.000001f;

void MacroActionAudio::FadeSourceVolume()
{
	auto s = obs_weak_source_get_source(_audioSource);
	if (!s) {
		return;
	}
	float vol = (float)_volume / 100.0f;
	float curVol = obs_source_get_volume(s);
	obs_source_release(s);
	bool volIncrease = curVol <= vol;
	int nrSteps = _duration.seconds * 1000 / fadeInterval.count();
	float volDiff = (volIncrease) ? vol - curVol : curVol - vol;
	float volStep = volDiff / nrSteps;

	if (volStep < minFade) {
		switcher->activeAudioFades[GetWeakSourceName(_audioSource)] =
			false;
		return;
	}

	auto macro = GetMacro();
	for (int step = 0; step < nrSteps && !macro->GetStop(); ++step) {
		auto s = obs_weak_source_get_source(_audioSource);
		if (!s) {
			return;
		}
		curVol = (volIncrease) ? curVol + volStep : curVol - volStep;
		obs_source_set_volume(s, curVol);
		std::this_thread::sleep_for(fadeInterval);
		obs_source_release(s);
	}

	switcher->activeAudioFades[GetWeakSourceName(_audioSource)] = false;
}
void MacroActionAudio::FadeMasterVolume()
{
	float vol = (float)_volume / 100.0f;
	float curVol = obs_get_master_volume();
	bool volIncrease = curVol <= vol;
	int nrSteps = _duration.seconds * 1000 / fadeInterval.count();
	float volDiff = (volIncrease) ? vol - curVol : curVol - vol;
	float volStep = volDiff / nrSteps;

	if (volStep < minFade) {
		switcher->masterAudioFadeActive = false;
		return;
	}

	auto macro = GetMacro();
	for (int step = 0; step < nrSteps && !macro->GetStop(); ++step) {
		curVol = (volIncrease) ? curVol + volStep : curVol - volStep;
		obs_set_master_volume(curVol);
		std::this_thread::sleep_for(fadeInterval);
	}

	switcher->masterAudioFadeActive = false;
}

void MacroActionAudio::StartSourceFade()
{
	if (!_audioSource) {
		return;
	}
	auto it = switcher->activeAudioFades.find(
		GetWeakSourceName(_audioSource));
	if (it != switcher->activeAudioFades.end() && it->second == true) {
		blog(LOG_WARNING,
		     "Audio fade for volume of %s already active! New fade request will be ignored!",
		     GetWeakSourceName(_audioSource).c_str());
		return;
	}
	switcher->activeAudioFades[GetWeakSourceName(_audioSource)] = true;
	if (_wait) {
		FadeSourceVolume();
	} else {
		GetMacro()->AddHelperThread(
			std::thread(&MacroActionAudio::FadeSourceVolume, this));
	}
}

void MacroActionAudio::StartMasterFade()
{

	if (switcher->masterAudioFadeActive) {
		blog(LOG_WARNING,
		     "Audio fade for master volume already active! New fade request will be ignored!");
		return;
	}
	switcher->masterAudioFadeActive = true;
	if (_wait) {
		FadeMasterVolume();
	} else {
		GetMacro()->AddHelperThread(
			std::thread(&MacroActionAudio::FadeMasterVolume, this));
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
		if (_fade && _duration.seconds != 0) {
			StartSourceFade();
		} else {
			obs_source_set_volume(s, (float)_volume / 100.0f);
		}
		break;
	case AudioAction::MASTER_VOLUME:
		if (_fade && _duration.seconds != 0) {
			StartMasterFade();
		} else {
			obs_set_master_volume((float)_volume / 100.0f);
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
	obs_data_set_bool(obj, "fade", _fade);
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
	_fade = obs_data_get_bool(obj, "fade");
	if (obs_data_has_user_value(obj, "wait")) {
		_wait = obs_data_get_bool(obj, "wait");
	} else {
		_wait = false;
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

MacroActionAudioEdit::MacroActionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroActionAudio> entryData)
	: QWidget(parent)
{
	_audioSources = new QComboBox();
	_actions = new QComboBox();
	_volumePercent = new QSpinBox();
	_volumePercent->setMinimum(0);
	_volumePercent->setMaximum(2000);
	_volumePercent->setSuffix("%");
	_fade = new QCheckBox();
	_wait = new QCheckBox();
	_duration = new DurationSelection(parent, false);

	populateActionSelection(_actions);
	populateAudioSelection(_audioSources);

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
	QWidget::connect(_wait, SIGNAL(stateChanged(int)), this,
			 SLOT(WaitChanged(int)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", _audioSources}, {"{{actions}}", _actions},
		{"{{volume}}", _volumePercent},      {"{{fade}}", _fade},
		{"{{duration}}", _duration},         {"{{wait}}", _wait},
	};
	QHBoxLayout *entryLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.audio.entry"),
		     entryLayout, widgetPlaceholders);
	_fadeLayout = new QHBoxLayout;
	placeWidgets(obs_module_text("AdvSceneSwitcher.action.audio.fade"),
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
	_wait->setChecked(_entryData->_wait);
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

void MacroActionAudioEdit::WaitChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_wait = value;
}
