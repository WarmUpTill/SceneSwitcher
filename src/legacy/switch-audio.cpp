#include <float.h>

#include "advanced-scene-switcher.hpp"
#include "volume-control.hpp"
#include "utility.hpp"

namespace advss {

bool AudioSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_audioAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->audioSwitches.emplace_back();

	AudioSwitchWidget *sw =
		new AudioSwitchWidget(this, &switcher->audioSwitches.back());

	listAddClicked(ui->audioSwitches, sw, ui->audioAdd, &addPulse);

	ui->audioHelp->setVisible(false);
}

void AdvSceneSwitcher::on_audioRemove_clicked()
{
	QListWidgetItem *item = ui->audioSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->audioSwitches->currentRow();
		auto &switches = switcher->audioSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_audioUp_clicked()
{
	int index = ui->audioSwitches->currentRow();
	if (!listMoveUp(ui->audioSwitches)) {
		return;
	}

	AudioSwitchWidget *s1 =
		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index));
	AudioSwitchWidget *s2 =
		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index - 1));
	AudioSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->audioSwitches[index],
		  switcher->audioSwitches[index - 1]);
}

void AdvSceneSwitcher::on_audioDown_clicked()
{
	int index = ui->audioSwitches->currentRow();

	if (!listMoveDown(ui->audioSwitches)) {
		return;
	}

	AudioSwitchWidget *s1 =
		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index));
	AudioSwitchWidget *s2 =
		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index + 1));
	AudioSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->audioSwitches[index],
		  switcher->audioSwitches[index + 1]);
}

void AdvSceneSwitcher::on_audioFallback_toggled(bool on)
{
	if (loading || !switcher) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->audioFallback.enable = on;
}

void SwitcherData::checkAudioSwitchFallback(OBSWeakSource &scene,
					    OBSWeakSource &transition)
{
	if (audioFallback.duration.DurationReached()) {
		scene = audioFallback.getScene();
		transition = audioFallback.transition;

		if (verbose) {
			audioFallback.logMatch();
		}
	}
}

bool SwitcherData::checkAudioSwitch(OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (AudioSwitch::pause) {
		return false;
	}

	bool match = false;
	bool fallbackChecked = false; // false if only one or no match

	for (AudioSwitch &s : audioSwitches) {
		if (!s.initialized()) {
			continue;
		}

		if (s.ignoreInactiveSource) {
			obs_source_t *as =
				obs_weak_source_get_source(s.audioSource);
			bool audioActive = obs_source_active(as);
			obs_source_release(as);

			if (!audioActive) {
				continue;
			}
		}

		// peak will have a value from -60 db to 0 db
		bool volumeThresholdreached = false;

		if (s.condition == ABOVE) {
			volumeThresholdreached = ((double)s.peak + 60) * 1.7 >
						 s.volumeThreshold;
		} else {
			volumeThresholdreached = ((double)s.peak + 60) * 1.7 <
						 s.volumeThreshold;
		}

		// Reset for next check
		s.peak = -FLT_MAX;

		if (!volumeThresholdreached) {
			s.duration.Reset();
		}

		if (volumeThresholdreached && s.duration.DurationReached()) {
			if (match) {
				checkAudioSwitchFallback(scene, transition);
				fallbackChecked = true;
				break;
			}

			scene = s.getScene();
			transition = s.transition;
			match = true;

			if (verbose) {
				s.logMatch();
			}

			if (!audioFallback.enable) {
				break;
			}
		}
	}

	if (!fallbackChecked) {
		audioFallback.duration.Reset();
	}

	return match;
}

void SwitcherData::saveAudioSwitches(obs_data_t *obj)
{
	obs_data_array_t *audioArray = obs_data_array_create();
	for (AudioSwitch &s : audioSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(audioArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "audioSwitches", audioArray);
	obs_data_array_release(audioArray);

	audioFallback.save(obj);
}

void SwitcherData::loadAudioSwitches(obs_data_t *obj)
{
	audioSwitches.clear();

	obs_data_array_t *audioArray = obs_data_get_array(obj, "audioSwitches");
	size_t count = obs_data_array_count(audioArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(audioArray, i);

		audioSwitches.emplace_back();
		audioSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(audioArray);

	audioFallback.load(obj);
}

void AdvSceneSwitcher::SetupAudioTab()
{
	for (auto &s : switcher->audioSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->audioSwitches);
		ui->audioSwitches->addItem(item);
		AudioSwitchWidget *sw = new AudioSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->audioSwitches->setItemWidget(item, sw);
	}

	if (switcher->audioSwitches.size() == 0) {
		if (!switcher->disableHints) {
			addPulse = PulseWidget(ui->audioAdd, QColor(Qt::green));
		}
		ui->audioHelp->setVisible(true);
	} else {
		ui->audioHelp->setVisible(false);
	}

	AudioSwitchFallbackWidget *fb =
		new AudioSwitchFallbackWidget(this, &switcher->audioFallback);
	ui->audioFallbackLayout->addWidget(fb);
	ui->audioFallback->setChecked(switcher->audioFallback.enable);
}

void AudioSwitch::setVolumeLevel(void *data, const float *,
				 const float peak[MAX_AUDIO_CHANNELS],
				 const float *)
{
	AudioSwitch *s = static_cast<AudioSwitch *>(data);

	for (int i = 0; i < MAX_AUDIO_CHANNELS; i++) {
		if (peak[i] > s->peak) {
			s->peak = peak[i];
		}
	}
}

obs_volmeter_t *AddVolmeterToSource(AudioSwitch *entry, obs_weak_source *source)
{
	obs_volmeter_t *volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, AudioSwitch::setVolumeLevel, entry);
	obs_source_t *as = obs_weak_source_get_source(source);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);

	return volmeter;
}

void AudioSwitch::resetVolmeter()
{
	obs_volmeter_remove_callback(volmeter, setVolumeLevel, this);
	obs_volmeter_destroy(volmeter);

	volmeter = AddVolmeterToSource(this, audioSource);
}

bool AudioSwitch::initialized()
{
	return SceneSwitcherEntry::initialized() && audioSource;
}

bool AudioSwitch::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(audioSource));
}

void AudioSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_string(obj, "audioSource",
			    GetWeakSourceName(audioSource).c_str());

	obs_data_set_int(obj, "volume", volumeThreshold);
	obs_data_set_int(obj, "condition", condition);
	duration.Save(obj, "duration");
	obs_data_set_bool(obj, "ignoreInactiveSource", ignoreInactiveSource);
}

void AudioSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	const char *audioSourceName = obs_data_get_string(obj, "audioSource");
	audioSource = GetWeakSourceByName(audioSourceName);

	volumeThreshold = obs_data_get_int(obj, "volume");
	condition = (audioCondition)obs_data_get_int(obj, "condition");
	duration.Load(obj, "duration");
	ignoreInactiveSource = obs_data_get_bool(obj, "ignoreInactiveSource");

	volmeter = AddVolmeterToSource(this, audioSource);
}

void AudioSwitchFallback::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj, "audioFallbackTargetType",
				 "audioFallbackScene",
				 "audioFallbackTransition");

	obs_data_set_bool(obj, "audioFallbackEnable", enable);
	duration.Save(obj, "audioFallbackDuration");
}

void AudioSwitchFallback::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj, "audioFallbackTargetType",
				 "audioFallbackScene",
				 "audioFallbackTransition");

	enable = obs_data_get_bool(obj, "audioFallbackEnable");
	duration.Load(obj, "audioFallbackDuration");
}

AudioSwitch::AudioSwitch(const AudioSwitch &other)
	: SceneSwitcherEntry(other.targetType, other.group, other.scene,
			     other.transition, other.usePreviousScene),
	  audioSource(other.audioSource),
	  volumeThreshold(other.volumeThreshold),
	  condition(other.condition),
	  duration(other.duration)
{
	volmeter = AddVolmeterToSource(this, other.audioSource);
}

AudioSwitch::AudioSwitch(AudioSwitch &&other) noexcept
	: SceneSwitcherEntry(other.targetType, other.group, other.scene,
			     other.transition, other.usePreviousScene),
	  audioSource(other.audioSource),
	  volumeThreshold(other.volumeThreshold),
	  condition(other.condition),
	  duration(other.duration),
	  volmeter(other.volmeter)
{
	other.volmeter = nullptr;
}

AudioSwitch::~AudioSwitch()
{
	obs_volmeter_remove_callback(volmeter, setVolumeLevel, this);
	obs_volmeter_destroy(volmeter);
}

AudioSwitch &AudioSwitch::operator=(const AudioSwitch &other)
{
	AudioSwitch t(other);
	swap(*this, t);
	return *this = AudioSwitch(other);
}

AudioSwitch &AudioSwitch::operator=(AudioSwitch &&other) noexcept
{
	if (this == &other) {
		return *this;
	}

	swap(*this, other);

	obs_volmeter_remove_callback(other.volmeter, setVolumeLevel, this);
	obs_volmeter_destroy(other.volmeter);
	other.volmeter = nullptr;

	return *this;
}

void swap(AudioSwitch &first, AudioSwitch &second)
{
	std::swap(first.targetType, second.targetType);
	std::swap(first.group, second.group);
	std::swap(first.scene, second.scene);
	std::swap(first.transition, second.transition);
	std::swap(first.usePreviousScene, second.usePreviousScene);
	std::swap(first.audioSource, second.audioSource);
	std::swap(first.volumeThreshold, second.volumeThreshold);
	std::swap(first.condition, second.condition);
	std::swap(first.duration, second.duration);
	std::swap(first.peak, second.peak);
	std::swap(first.volmeter, second.volmeter);
	first.resetVolmeter();
	second.resetVolmeter();
}

static inline void populateConditionSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audioTab.condition.above"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audioTab.condition.below"));
}

AudioSwitchWidget::AudioSwitchWidget(QWidget *parent, AudioSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	audioSources = new QComboBox();
	condition = new QComboBox();
	audioVolumeThreshold = new QSpinBox();
	duration = new DurationSelection(this, false);
	ignoreInactiveSource = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.audioTab.ignoreInactiveSource"));

	obs_source_t *source = nullptr;
	if (s) {
		source = obs_weak_source_get_source(s->audioSource);
	}
	volMeter = new VolControl(source);
	obs_source_release(source);

	audioVolumeThreshold->setSuffix("%");
	audioVolumeThreshold->setMaximum(100);
	audioVolumeThreshold->setMinimum(0);

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeThresholdChanged(int)));
	QWidget::connect(condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));
	QWidget::connect(audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(ignoreInactiveSource, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreInactiveChanged(int)));

	populateAudioSelection(audioSources);
	populateConditionSelection(condition);

	if (s) {
		audioSources->setCurrentText(
			GetWeakSourceName(s->audioSource).c_str());
		audioVolumeThreshold->setValue(s->volumeThreshold);
		condition->setCurrentIndex(s->condition);
		duration->SetDuration(s->duration);
		ignoreInactiveSource->setChecked(s->ignoreInactiveSource);
	}

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", audioSources},
		{"{{volumeWidget}}", audioVolumeThreshold},
		{"{{condition}}", condition},
		{"{{duration}}", duration},
		{"{{ignoreInactiveSource}}", ignoreInactiveSource},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.audioTab.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(volMeter);

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

AudioSwitch *AudioSwitchWidget::getSwitchData()
{
	return switchData;
}

void AudioSwitchWidget::setSwitchData(AudioSwitch *s)
{
	switchData = s;
}

void AudioSwitchWidget::swapSwitchData(AudioSwitchWidget *s1,
				       AudioSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	AudioSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void AudioSwitchWidget::UpdateVolmeterSource()
{
	delete volMeter;
	obs_source_t *soruce =
		obs_weak_source_get_source(switchData->audioSource);
	volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	QLayout *layout = this->layout();
	layout->addWidget(volMeter);

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));

	// Slider will default to 0 so set it manually once
	volMeter->GetSlider()->setValue(switchData->volumeThreshold);
}

void AudioSwitchWidget::SourceChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->audioSource = GetWeakSourceByQString(text);
	switchData->resetVolmeter();
	UpdateVolmeterSource();
}

void AudioSwitchWidget::VolumeThresholdChanged(int vol)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->volumeThreshold = vol;
}

void AudioSwitchWidget::ConditionChanged(int cond)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->condition = (audioCondition)cond;
}

void AudioSwitchWidget::DurationChanged(const Duration &dur)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

void AudioSwitchWidget::IgnoreInactiveChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->ignoreInactiveSource = state;
}

AudioSwitchFallbackWidget::AudioSwitchFallbackWidget(QWidget *parent,
						     AudioSwitchFallback *s)
	: SwitchWidget(parent, s, true, true)
{
	duration = new DurationSelection(this, false);

	QWidget::connect(duration, SIGNAL(DurationChanged(const Duration &)),
			 this, SLOT(DurationChanged(const Duration &)));

	if (s) {
		duration->SetDuration(s->duration);
	}

	QHBoxLayout *mainLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{scenes}}", scenes},
		{"{{duration}}", duration},
		{"{{transitions}}", transitions}};
	placeWidgets(
		obs_module_text("AdvSceneSwitcher.audioTab.multiMatchfallback"),
		mainLayout, widgetPlaceholders);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

void AudioSwitchFallbackWidget::DurationChanged(const Duration &dur)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

} // namespace advss
