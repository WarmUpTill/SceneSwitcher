#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-audio.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

const std::string MacroConditionAudio::id = "audio";

bool MacroConditionAudio::_registered = MacroConditionFactory::Register(
	MacroConditionAudio::id,
	{MacroConditionAudio::Create, MacroConditionAudioEdit::Create,
	 "AdvSceneSwitcher.condition.audio"});

static std::map<AudioCondition, std::string> audioConditionTypes = {
	{AudioCondition::ABOVE, "AdvSceneSwitcher.condition.audio.state.above"},
	{AudioCondition::BELOW, "AdvSceneSwitcher.condition.audio.state.below"},
	{AudioCondition::MUTE, "AdvSceneSwitcher.condition.audio.state.mute"},
	{AudioCondition::UNMUTE,
	 "AdvSceneSwitcher.condition.audio.state.unmute"},
};

MacroConditionAudio::~MacroConditionAudio()
{
	obs_volmeter_remove_callback(_volmeter, SetVolumeLevel, this);
	obs_volmeter_destroy(_volmeter);
}

bool MacroConditionAudio::CheckCondition()
{
	bool ret = false;
	auto s = obs_weak_source_get_source(_audioSource);
	switch (_condition) {
	case AudioCondition::ABOVE:
		// peak will have a value from -60 db to 0 db
		ret = ((double)_peak + 60) * 1.7 > _volume;
		break;
	case AudioCondition::BELOW:
		// peak will have a value from -60 db to 0 db
		ret = ((double)_peak + 60) * 1.7 < _volume;
		break;
	case AudioCondition::MUTE:
		ret = obs_source_muted(s);
		break;
	case AudioCondition::UNMUTE:
		ret = !obs_source_muted(s);
		break;
	default:
		break;
	}

	// Reset for next check
	_peak = -std::numeric_limits<float>::infinity();

	obs_source_release(s);
	return ret;
}

bool MacroConditionAudio::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(_audioSource).c_str());
	obs_data_set_int(obj, "volume", _volume);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
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

bool MacroConditionAudio::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	_audioSource = GetWeakSourceByName(audioSourceName);
	_volume = obs_data_get_int(obj, "volume");
	_condition =
		static_cast<AudioCondition>(obs_data_get_int(obj, "condition"));
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

void MacroConditionAudio::SetVolumeLevel(
	void *data, const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS],
	const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(magnitude);
	UNUSED_PARAMETER(inputPeak);
	MacroConditionAudio *c = static_cast<MacroConditionAudio *>(data);

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

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : audioConditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

MacroConditionAudioEdit::MacroConditionAudioEdit(
	QWidget *parent, std::shared_ptr<MacroConditionAudio> entryData)
	: QWidget(parent)
{
	_audioSources = new QComboBox();
	_condition = new QComboBox();
	_volume = new QSpinBox();

	_volume->setSuffix("%");
	_volume->setMaximum(100);
	_volume->setMinimum(0);

	QWidget::connect(_volume, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeThresholdChanged(int)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	populateAudioSelection(_audioSources);
	populateConditionSelection(_condition);

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
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
	_entryData->_condition = static_cast<AudioCondition>(cond);
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
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	UpdateVolmeterSource();
	SetWidgetVisibility();
}

void MacroConditionAudioEdit::SetWidgetVisibility()
{
	if (!_entryData) {
		return;
	}

	_volume->setVisible(_entryData->_condition == AudioCondition::ABOVE ||
			    _entryData->_condition == AudioCondition::BELOW);
	_volMeter->setVisible(_entryData->_condition == AudioCondition::ABOVE ||
			      _entryData->_condition == AudioCondition::BELOW);
	adjustSize();
}
