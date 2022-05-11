#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-audio.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionAudio::id = "audio";

bool MacroConditionAudio::_registered = MacroConditionFactory::Register(
	MacroConditionAudio::id,
	{MacroConditionAudio::Create, MacroConditionAudioEdit::Create,
	 "AdvSceneSwitcher.condition.audio"});

static std::map<AudioOutputCondition, std::string> audioOutputConditionTypes = {
	{AudioOutputCondition::ABOVE,
	 "AdvSceneSwitcher.condition.audio.state.above"},
	{AudioOutputCondition::BELOW,
	 "AdvSceneSwitcher.condition.audio.state.below"},
};

static std::map<AudioVolumeCondition, std::string> audioVolumeConditionTypes = {
	{AudioVolumeCondition::ABOVE,
	 "AdvSceneSwitcher.condition.audio.state.above"},
	{AudioVolumeCondition::EXACT,
	 "AdvSceneSwitcher.condition.audio.state.exact"},
	{AudioVolumeCondition::BELOW,
	 "AdvSceneSwitcher.condition.audio.state.below"},
	{AudioVolumeCondition::MUTE,
	 "AdvSceneSwitcher.condition.audio.state.mute"},
	{AudioVolumeCondition::UNMUTE,
	 "AdvSceneSwitcher.condition.audio.state.unmute"},
};

MacroConditionAudio::~MacroConditionAudio()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);
}

bool MacroConditionAudio::CheckOutputCondition()
{
	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource);

	switch (_outputCondition) {
	case AudioOutputCondition::ABOVE:
		// peak will have a value from -60 db to 0 db
		ret = ((double)_peak + 60) * 1.7 > _volume;
		break;
	case AudioOutputCondition::BELOW:
		// peak will have a value from -60 db to 0 db
		ret = ((double)_peak + 60) * 1.7 < _volume;
		break;
	default:
		break;
	}

	// Reset for next check
	_peak = -std::numeric_limits<float>::infinity();
	obs_source_release(s);

	return ret;
}

bool MacroConditionAudio::CheckVolumeCondition()
{

	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource);

	switch (_volumeCondition) {
	case AudioVolumeCondition::ABOVE:
		ret = obs_source_get_volume(s) > _volume / 100.f;
		break;
	case AudioVolumeCondition::EXACT:
		ret = obs_source_get_volume(s) == _volume / 100.f;
		break;
	case AudioVolumeCondition::BELOW:
		ret = obs_source_get_volume(s) < _volume / 100.f;
		break;
	case AudioVolumeCondition::MUTE:
		ret = obs_source_muted(s);
		break;
	case AudioVolumeCondition::UNMUTE:
		ret = !obs_source_muted(s);
		break;
	default:
		break;
	}

	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::CheckCondition()
{
	switch (_checkType) {
	case AudioConditionCheckType::OUTPUT_VOLUME:
		return CheckOutputCondition();
	case AudioConditionCheckType::CONFIGURED_VOLUME:
		return CheckVolumeCondition();
	default:
		break;
	}
	return false;
}

bool MacroConditionAudio::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(_audioSource).c_str());
	obs_data_set_int(obj, "volume", _volume);
	obs_data_set_int(obj, "checkType", static_cast<int>(_checkType));
	obs_data_set_int(obj, "outputCondition",
			 static_cast<int>(_outputCondition));
	obs_data_set_int(obj, "volumeCondition",
			 static_cast<int>(_volumeCondition));
	return true;
}

obs_volmeter_t *AddVolmeterToSource(MacroConditionAudio *entry,
				    obs_weak_source *source)
{
	obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, MacroConditionAudio::SetVolumeLevel,
				  entry);
	obs_source_t *as = obs_weak_source_get_source(source);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);

	return volmeter;
}

// TODO: Remove in future version
static void convertOldSettingsFormat(obs_data_t *obj)
{
	auto oldCond = static_cast<AudioOutputCondition>(
		obs_data_get_int(obj, "condition"));
	switch (oldCond) {
	case AudioOutputCondition::ABOVE:
		obs_data_set_int(
			obj, "checkType",
			static_cast<int>(
				AudioConditionCheckType::OUTPUT_VOLUME));
		obs_data_set_int(obj, "outputCondition",
				 static_cast<int>(AudioOutputCondition::ABOVE));
		break;
	case AudioOutputCondition::BELOW:
		obs_data_set_int(
			obj, "checkType",
			static_cast<int>(
				AudioConditionCheckType::OUTPUT_VOLUME));
		obs_data_set_int(obj, "outputCondition",
				 static_cast<int>(AudioOutputCondition::BELOW));
		break;
	case AudioOutputCondition::MUTE:
		obs_data_set_int(
			obj, "checkType",
			static_cast<int>(
				AudioConditionCheckType::CONFIGURED_VOLUME));
		obs_data_set_int(obj, "volumeCondition",
				 static_cast<int>(AudioVolumeCondition::MUTE));
		break;
	case AudioOutputCondition::UNMUTE:
		obs_data_set_int(
			obj, "checkType",
			static_cast<int>(
				AudioConditionCheckType::CONFIGURED_VOLUME));
		obs_data_set_int(
			obj, "volumeCondition",
			static_cast<int>(AudioVolumeCondition::UNMUTE));
		break;
	default:
		break;
	}
}

bool MacroConditionAudio::Load(obs_data_t *obj)
{
	if (!obs_data_has_user_value(obj, "checkType")) {
		convertOldSettingsFormat(obj);
	}

	MacroCondition::Load(obj);
	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	_audioSource = GetWeakSourceByName(audioSourceName);
	_volume = obs_data_get_int(obj, "volume");
	_checkType = static_cast<AudioConditionCheckType>(
		obs_data_get_int(obj, "checkType"));
	_outputCondition = static_cast<AudioOutputCondition>(
		obs_data_get_int(obj, "outputCondition"));
	_volumeCondition = static_cast<AudioVolumeCondition>(
		obs_data_get_int(obj, "volumeCondition"));
	_volmeter = AddVolmeterToSource(this, _audioSource);
	return true;
}

std::string MacroConditionAudio::GetShortDesc()
{
	if (_audioSource) {
		return GetWeakSourceName(_audioSource);
	}
	return "";
}

void MacroConditionAudio::SetVolumeLevel(void *data, const float *,
					 const float peak[MAX_AUDIO_CHANNELS],
					 const float *)
{
	MacroConditionAudio *c = static_cast<MacroConditionAudio *>(data);
	const auto macro = c->GetMacro();
	if (macro && macro->Paused()) {
		return;
	}

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (peak[i] > c->_peak) {
			c->_peak = peak[i];
		}
	}
}

void MacroConditionAudio::ResetVolmeter()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);

	_volmeter = AddVolmeterToSource(this, _audioSource);
}

static inline void populateCheckTypes(QComboBox *list)
{
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.audio.type.output"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.condition.audio.type.volume"));
}

static inline void populateOutputConditionSelection(QComboBox *list)
{
	list->clear();
	for (auto entry : audioOutputConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populateVolumeConditionSelection(QComboBox *list)
{
	list->clear();
	for (auto entry : audioVolumeConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionAudioEdit::MacroConditionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroConditionAudio> entryData)
	: QWidget(parent)
{
	_checkTypes = new QComboBox();
	_audioSources = new QComboBox();
	_condition = new QComboBox();
	_volume = new QSpinBox();

	_volume->setSuffix("%");
	_volume->setMaximum(100);
	_volume->setMinimum(0);

	QWidget::connect(_checkTypes, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(CheckTypeChanged(int)));
	QWidget::connect(_volume, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeThresholdChanged(int)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	populateCheckTypes(_checkTypes);
	populateAudioSelection(_audioSources);

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkType}}", _checkTypes},
		{"{{audioSources}}", _audioSources},
		{"{{volume}}", _volume},
		{"{{condition}}", _condition},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.audio.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);

	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionAudioEdit::UpdateVolmeterSource()
{
	delete _volMeter;
	obs_source_t *soruce =
		obs_weak_source_get_source(_entryData->_audioSource);
	_volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	QLayout *layout = this->layout();
	layout->addWidget(_volMeter);

	QWidget::connect(_volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 _volume, SLOT(setValue(int)));
	QWidget::connect(_volume, SIGNAL(valueChanged(int)),
			 _volMeter->GetSlider(), SLOT(setValue(int)));

	// Slider will default to 0 so set it manually once
	_volMeter->GetSlider()->setValue(_entryData->_volume);
}

void MacroConditionAudioEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_audioSource = GetWeakSourceByQString(text);
	_entryData->ResetVolmeter();
	UpdateVolmeterSource();
	SetWidgetVisibility();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionAudioEdit::VolumeThresholdChanged(int vol)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_volume = vol;
}

void MacroConditionAudioEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	if (_entryData->_checkType == AudioConditionCheckType::OUTPUT_VOLUME) {
		_entryData->_outputCondition =
			static_cast<AudioOutputCondition>(cond);
	} else {
		_entryData->_volumeCondition =
			static_cast<AudioVolumeCondition>(cond);
	}
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::CheckTypeChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_checkType = static_cast<AudioConditionCheckType>(cond);

	const QSignalBlocker b(_condition);
	if (_entryData->_checkType == AudioConditionCheckType::OUTPUT_VOLUME) {
		populateOutputConditionSelection(_condition);
	} else {
		populateVolumeConditionSelection(_condition);
	}
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_audioSources->setCurrentText(
		GetWeakSourceName(_entryData->_audioSource).c_str());
	_volume->setValue(_entryData->_volume);
	_checkTypes->setCurrentIndex(static_cast<int>(_entryData->_checkType));
	if (_entryData->_checkType == AudioConditionCheckType::OUTPUT_VOLUME) {
		populateOutputConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_outputCondition));
	} else {
		populateVolumeConditionSelection(_condition);
		_condition->setCurrentIndex(
			static_cast<int>(_entryData->_volumeCondition));
	}
	UpdateVolmeterSource();
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_volume->setVisible(
		_entryData->_checkType ==
			AudioConditionCheckType::OUTPUT_VOLUME ||
		(_entryData->_checkType ==
			 AudioConditionCheckType::CONFIGURED_VOLUME &&
		 (_entryData->_volumeCondition == AudioVolumeCondition::ABOVE ||
		  _entryData->_volumeCondition == AudioVolumeCondition::EXACT ||
		  _entryData->_volumeCondition == AudioVolumeCondition::BELOW)));
	_volMeter->setVisible(_entryData->_checkType ==
			      AudioConditionCheckType::OUTPUT_VOLUME);
	adjustSize();
}
