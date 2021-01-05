#include "headers/advanced-scene-switcher.hpp"
#include "headers/volume-control.hpp"
#include "headers/utility.hpp"

bool AudioSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_audioAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->audioSwitches.emplace_back();

	listAddClicked(ui->audioSwitches,
		       new AudioSwitchWidget(&switcher->audioSwitches.back()),
		       ui->audioAdd, &addPulse);
}

void AdvSceneSwitcher::on_audioRemove_clicked()
{
	QListWidgetItem *item = ui->audioSwitches->currentItem();
	if (!item)
		return;

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
	if (!listMoveUp(ui->audioSwitches))
		return;

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

	if (!listMoveDown(ui->audioSwitches))
		return;

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
	if (loading || !switcher)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->audioFallback.enable = on;
}

void SwitcherData::checkAudioSwitchFallback(OBSWeakSource &scene,
					    OBSWeakSource &transition)
{
	bool durationReached =
		((unsigned long long)audioFallback.matchCount * interval) /
			1000.0 >=
		audioFallback.duration;

	if (durationReached) {

		scene = (audioFallback.usePreviousScene) ? previousScene
							 : audioFallback.scene;
		transition = audioFallback.transition;

		if (verbose)
			audioFallback.logMatch();
	}

	audioFallback.matchCount++;
}

void SwitcherData::checkAudioSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (AudioSwitch::pause)
		return;

	bool fallbackChecked = false; // false if one or no match

	for (AudioSwitch &s : audioSwitches) {
		if (!s.initialized())
			continue;

		obs_source_t *as = obs_weak_source_get_source(s.audioSource);
		bool audioActive = obs_source_active(as);
		obs_source_release(as);

		// peak will have a value from -60 db to 0 db
		bool volumeThresholdreached = false;

		if (s.condition == ABOVE)
			volumeThresholdreached = ((double)s.peak + 60) * 1.7 >
						 s.volumeThreshold;
		else
			volumeThresholdreached = ((double)s.peak + 60) * 1.7 <
						 s.volumeThreshold;

		if (volumeThresholdreached) {
			s.matchCount++;
		} else {
			s.matchCount = 0;
		}

		bool durationReached =
			((unsigned long long)s.matchCount * interval) /
				1000.0 >=
			s.duration;

		if (volumeThresholdreached && durationReached && audioActive) {
			if (match) {
				checkAudioSwitchFallback(scene, transition);
				fallbackChecked = true;
				break;
			}

			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				s.logMatch();

			if (!audioFallback.enable)
				break;
		}
	}

	if (!fallbackChecked)
		audioFallback.matchCount = 0;
}

void saveAudioFallback(obs_data_t *obj, AudioSwitchFallback &audioFallback)
{
	obs_source_t *fallbackSceneSource =
		obs_weak_source_get_source(audioFallback.scene);
	obs_source_t *fallbackTransition =
		obs_weak_source_get_source(audioFallback.transition);

	const char *fallbackSceneName =
		obs_source_get_name(fallbackSceneSource);
	const char *fallbackTransitionName =
		obs_source_get_name(fallbackTransition);

	obs_data_set_bool(obj, "audioFallbackEnable", audioFallback.enable);
	obs_data_set_string(obj, "audioFallbackScene",
			    audioFallback.usePreviousScene ? previous_scene_name
							   : fallbackSceneName);
	obs_data_set_string(obj, "audioFallbackTransition",
			    fallbackTransitionName);
	obs_data_set_double(obj, "audioFallbackDuration",
			    audioFallback.duration);

	obs_source_release(fallbackSceneSource);
	obs_source_release(fallbackTransition);
}

void SwitcherData::saveAudioSwitches(obs_data_t *obj)
{
	obs_data_array_t *audioArray = obs_data_array_create();
	for (AudioSwitch &s : switcher->audioSwitches) {
		obs_data_t *array_obj = obs_data_create();

		obs_source_t *sceneSource = obs_weak_source_get_source(s.scene);
		obs_source_t *transition =
			obs_weak_source_get_source(s.transition);
		obs_source_t *audioSrouce =
			obs_weak_source_get_source(s.audioSource);
		if ((s.usePreviousScene || sceneSource) && transition &&
		    audioSrouce) {
			const char *sceneName =
				obs_source_get_name(sceneSource);
			const char *transitionName =
				obs_source_get_name(transition);
			const char *audioSourceName =
				obs_source_get_name(audioSrouce);
			obs_data_set_string(array_obj, "scene",
					    s.usePreviousScene
						    ? previous_scene_name
						    : sceneName);
			obs_data_set_string(array_obj, "transition",
					    transitionName);
			obs_data_set_string(array_obj, "audioSource",
					    audioSourceName);
			obs_data_set_int(array_obj, "volume",
					 s.volumeThreshold);
			obs_data_set_int(array_obj, "condition", s.condition);
			obs_data_set_double(array_obj, "duration", s.duration);
			obs_data_array_push_back(audioArray, array_obj);
		}
		obs_source_release(sceneSource);
		obs_source_release(transition);
		obs_source_release(audioSrouce);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "audioSwitches", audioArray);
	obs_data_array_release(audioArray);

	saveAudioFallback(obj, audioFallback);
}

void loadAudioFallback(obs_data_t *obj, AudioSwitchFallback &audioFallback)
{
	const char *fallbackSceneName =
		obs_data_get_string(obj, "audioFallbackScene");
	const char *fallbackTransitionName =
		obs_data_get_string(obj, "audioFallbackTransition");

	audioFallback.enable = obs_data_get_bool(obj, "audioFallbackEnable");
	audioFallback.duration =
		obs_data_get_double(obj, "audioFallbackDuration");
	audioFallback.scene = GetWeakSourceByName(fallbackSceneName);
	audioFallback.transition =
		GetWeakTransitionByName(fallbackTransitionName);
	audioFallback.usePreviousScene =
		strcmp(fallbackSceneName, previous_scene_name) == 0;
}

void SwitcherData::loadAudioSwitches(obs_data_t *obj)
{
	switcher->audioSwitches.clear();

	obs_data_array_t *audioArray = obs_data_get_array(obj, "audioSwitches");
	size_t count = obs_data_array_count(audioArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(audioArray, i);

		const char *scene = obs_data_get_string(array_obj, "scene");
		const char *transition =
			obs_data_get_string(array_obj, "transition");
		const char *audioSource =
			obs_data_get_string(array_obj, "audioSource");
		int vol = obs_data_get_int(array_obj, "volume");
		audioCondition condition = (audioCondition)obs_data_get_int(
			array_obj, "condition");
		double duration = obs_data_get_double(array_obj, "duration");

		switcher->audioSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition),
			GetWeakSourceByName(audioSource), vol, condition,
			duration, (strcmp(scene, previous_scene_name) == 0));

		obs_data_release(array_obj);
	}
	obs_data_array_release(audioArray);

	loadAudioFallback(obj, audioFallback);
}

void AdvSceneSwitcher::setupAudioTab()
{
	for (auto &s : switcher->audioSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->audioSwitches);
		ui->audioSwitches->addItem(item);
		AudioSwitchWidget *sw = new AudioSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->audioSwitches->setItemWidget(item, sw);
	}

	if (switcher->audioSwitches.size() == 0)
		addPulse = PulseWidget(ui->audioAdd, QColor(Qt::green));

	AudioSwitchFallbackWidget *fb =
		new AudioSwitchFallbackWidget(&switcher->audioFallback);
	ui->audioFallbackLayout->addWidget(fb);
	ui->audioFallback->setChecked(switcher->audioFallback.enable);
}

void AudioSwitch::setVolumeLevel(void *data,
				 const float magnitude[MAX_AUDIO_CHANNELS],
				 const float peak[MAX_AUDIO_CHANNELS],
				 const float inputPeak[MAX_AUDIO_CHANNELS])
{
	UNUSED_PARAMETER(magnitude);
	UNUSED_PARAMETER(inputPeak);
	AudioSwitch *s = static_cast<AudioSwitch *>(data);

	s->peak = peak[0];
	for (int i = 1; i < MAX_AUDIO_CHANNELS; i++)
		if (peak[i] > s->peak)
			s->peak = peak[i];
}

void AudioSwitch::resetVolmeter()
{
	obs_volmeter_remove_callback(volmeter, setVolumeLevel, this);
	obs_volmeter_destroy(volmeter);

	volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, setVolumeLevel, this);
	obs_source_t *as = obs_weak_source_get_source(audioSource);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);
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

AudioSwitch::AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
			 OBSWeakSource audioSource_, int volumeThreshold_,
			 audioCondition condition_, double duration_,
			 bool usePreviousScene_)
	: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
	  audioSource(audioSource_),
	  volumeThreshold(volumeThreshold_),
	  condition(condition_),
	  duration(duration_)
{
	volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, setVolumeLevel, this);
	obs_source_t *as = obs_weak_source_get_source(audioSource);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);
}

AudioSwitch::AudioSwitch(const AudioSwitch &other)
	: SceneSwitcherEntry(other.scene, other.transition,
			     other.usePreviousScene),
	  audioSource(other.audioSource),
	  volumeThreshold(other.volumeThreshold),
	  condition(other.condition),
	  duration(other.duration)
{
	volmeter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(volmeter, setVolumeLevel, this);
	obs_source_t *as = obs_weak_source_get_source(other.audioSource);
	if (!obs_volmeter_attach_source(volmeter, as)) {
		const char *name = obs_source_get_name(as);
		blog(LOG_WARNING, "failed to attach volmeter to source %s",
		     name);
	}
	obs_source_release(as);
}

AudioSwitch::AudioSwitch(AudioSwitch &&other)
	: SceneSwitcherEntry(other.scene, other.transition,
			     other.usePreviousScene),
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

void populateConditionSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audioTab.condition.above"));
	list->addItem(
		obs_module_text("AdvSceneSwitcher.audioTab.condition.below"));
}

AudioSwitchWidget::AudioSwitchWidget(AudioSwitch *s) : SwitchWidget(s)
{
	audioSources = new QComboBox();
	condition = new QComboBox();
	audioVolumeThreshold = new QSpinBox();
	duration = new QDoubleSpinBox();

	obs_source_t *soruce = nullptr;
	if (s)
		soruce = obs_weak_source_get_source(s->audioSource);
	volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	audioVolumeThreshold->setSuffix("%");
	audioVolumeThreshold->setMaximum(100);
	audioVolumeThreshold->setMinimum(0);

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeThresholdChanged(int)));
	QWidget::connect(condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	AdvSceneSwitcher::populateAudioSelection(audioSources);
	populateConditionSelection(condition);

	if (s) {
		audioSources->setCurrentText(
			GetWeakSourceName(s->audioSource).c_str());
		audioVolumeThreshold->setValue(s->volumeThreshold);
		condition->setCurrentIndex(s->condition);
		duration->setValue(s->duration);
	}

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{audioSources}}", audioSources},
		{"{{volumeWidget}}", audioVolumeThreshold},
		{"{{condition}}", condition},
		{"{{duration}}", duration},
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
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->audioSource = GetWeakSourceByQString(text);
	switchData->resetVolmeter();
	UpdateVolmeterSource();
}

void AudioSwitchWidget::VolumeThresholdChanged(int vol)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->volumeThreshold = vol;
}

void AudioSwitchWidget::ConditionChanged(int cond)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->condition = (audioCondition)cond;
}

void AudioSwitchWidget::DurationChanged(double dur)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

AudioSwitchFallbackWidget::AudioSwitchFallbackWidget(AudioSwitchFallback *s)
	: SwitchWidget(s)
{
	duration = new QDoubleSpinBox();

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));

	if (s) {
		duration->setValue(s->duration);
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

void AudioSwitchFallbackWidget::DurationChanged(double dur)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}
