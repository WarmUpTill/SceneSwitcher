#include <QWidget>
#include <QHBoxLayout>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/volume-control.hpp"

void SceneSwitcher::on_audioAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->audioSwitches.emplace_back();

	QListWidgetItem *item;
	item = new QListWidgetItem(ui->audioSwitches);
	ui->audioSwitches->addItem(item);
	AudioSwitchWidget *sw =
		new AudioSwitchWidget(&switcher->audioSwitches.back());
	item->setSizeHint(sw->minimumSizeHint());
	ui->audioSwitches->setItemWidget(item, sw);
}

void SceneSwitcher::on_audioRemove_clicked()
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

void SceneSwitcher::on_audioUp_clicked()
{
	int index = ui->audioSwitches->currentRow();
	if (index != -1 && index != 0) {
		delete ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index));
		AudioSwitchWidget *sw =
			new AudioSwitchWidget(&switcher->audioSwitches[index]);

		ui->audioSwitches->insertItem(
			index - 1, ui->audioSwitches->takeItem(index));
		ui->audioSwitches->setItemWidget(
			ui->audioSwitches->item(index - 1), sw);

		ui->audioSwitches->setCurrentRow(index - 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		std::swap(switcher->audioSwitches[index],
			  switcher->audioSwitches[index - 1]);
	}
}

void SceneSwitcher::on_audioDown_clicked()
{
	int index = ui->audioSwitches->currentRow();
	if (index != -1 && index != ui->audioSwitches->count() - 1) {
		delete ui->audioSwitches->itemWidget(
			ui->audioSwitches->item(index));
		AudioSwitchWidget *sw =
			new AudioSwitchWidget(&switcher->audioSwitches[index]);

		ui->audioSwitches->insertItem(
			index + 1, ui->audioSwitches->takeItem(index));
		ui->audioSwitches->setItemWidget(
			ui->audioSwitches->item(index + 1), sw);

		ui->audioSwitches->setCurrentRow(index + 1);

		std::lock_guard<std::mutex> lock(switcher->m);

		std::swap(switcher->audioSwitches[index],
			  switcher->audioSwitches[index + 1]);
	}
}

void SwitcherData::checkAudioSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (audioSwitches.size() == 0)
		return;

	for (AudioSwitch &s : audioSwitches) {
		if (!s.initialized())
			continue;

		obs_source_t *as = obs_weak_source_get_source(s.audioSource);
		bool audioActive = obs_source_active(as);
		obs_source_release(as);

		// peak will have a value from -60 db to 0 db
		bool volumeThresholdreached = ((double)s.peak + 60) * 1.7 >
					      s.volumeThreshold;

		if (volumeThresholdreached && audioActive) {
			scene = (s.usePreviousScene) ? previousScene : s.scene;
			transition = s.transition;
			match = true;

			if (verbose)
				s.logMatch();
			break;
		}
	}
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
		if ((s.usePreviousScene || sceneSource) && transition) {
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
			obs_data_array_push_back(audioArray, array_obj);
		}
		obs_source_release(sceneSource);
		obs_source_release(transition);
		obs_source_release(audioSrouce);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "audioSwitches", audioArray);
	obs_data_array_release(audioArray);
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

		switcher->audioSwitches.emplace_back(
			GetWeakSourceByName(scene),
			GetWeakTransitionByName(transition),
			GetWeakSourceByName(audioSource), vol,
			(strcmp(scene, previous_scene_name) == 0));

		obs_data_release(array_obj);
	}
	obs_data_array_release(audioArray);
}

void SceneSwitcher::setupAudioTab()
{
	for (auto &s : switcher->audioSwitches) {
		std::string sceneName = (s.usePreviousScene)
						? previous_scene_name
						: GetWeakSourceName(s.scene);
		std::string transitionName = GetWeakSourceName(s.transition);
		std::string audioSourceName = GetWeakSourceName(s.audioSource);

		QListWidgetItem *item;
		item = new QListWidgetItem(ui->audioSwitches);
		ui->audioSwitches->addItem(item);
		AudioSwitchWidget *sw = new AudioSwitchWidget(&s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->audioSwitches->setItemWidget(item, sw);
	}
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
	return audioSource && SceneSwitcherEntry::initialized();
}

bool AudioSwitch::valid()
{
	return !initialized() ||
	       ((usePreviousScene || WeakSourceValid(scene)) &&
		WeakSourceValid(audioSource) && WeakSourceValid(transition));
}

inline AudioSwitch::AudioSwitch(OBSWeakSource scene_, OBSWeakSource transition_,
				OBSWeakSource audioSource_,
				int volumeThreshold_, bool usePreviousScene_)
	: SceneSwitcherEntry(scene_, transition_, usePreviousScene_),
	  audioSource(audioSource_),
	  volumeThreshold(volumeThreshold_)
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
	  volumeThreshold(other.volumeThreshold)
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
	  volmeter(other.volmeter)
{
	other.volmeter = nullptr;
}

inline AudioSwitch::~AudioSwitch()
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
	std::swap(first.peak, second.peak);
	std::swap(first.volmeter, second.volmeter);
	first.resetVolmeter();
	second.resetVolmeter();
}

AudioSwitchWidget::AudioSwitchWidget(AudioSwitch *s)
{
	audioScenes = new QComboBox();
	audioTransitions = new QComboBox();
	audioSources = new QComboBox();
	audioVolumeThreshold = new QSpinBox();

	obs_source_t *soruce = nullptr;
	if (s)
		soruce = obs_weak_source_get_source(s->audioSource);
	volMeter = new VolControl(soruce);
	obs_source_release(soruce);

	whenLabel = new QLabel("When the volume of");
	aboveLabel = new QLabel("is above");
	switchLabel = new QLabel("switch to");
	usingLabel = new QLabel("using");

	audioVolumeThreshold->setSuffix("%");
	audioVolumeThreshold->setMaximum(100);
	audioVolumeThreshold->setMinimum(0);

	QWidget::connect(volMeter->GetSlider(), SIGNAL(valueChanged(int)),
			 audioVolumeThreshold, SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)),
			 volMeter->GetSlider(), SLOT(setValue(int)));
	QWidget::connect(audioVolumeThreshold, SIGNAL(valueChanged(int)), this,
			 SLOT(VolumeThresholdChanged(int)));
	QWidget::connect(audioScenes,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SceneChanged(const QString &)));
	QWidget::connect(audioTransitions,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(TransitionChanged(const QString &)));
	QWidget::connect(audioSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	SceneSwitcher::populateSceneSelection(audioScenes, true);
	SceneSwitcher::populateTransitionSelection(audioTransitions);
	SceneSwitcher::populateAudioSelection(audioSources);

	if (s) {
		audioScenes->setCurrentText(
			GetWeakSourceName(s->scene).c_str());
		audioTransitions->setCurrentText(
			GetWeakSourceName(s->transition).c_str());
		audioSources->setCurrentText(
			GetWeakSourceName(s->audioSource).c_str());
		audioVolumeThreshold->setValue(s->volumeThreshold);
	}

	setStyleSheet("* { background-color: transparent; }");

	QHBoxLayout *switchLayout = new QHBoxLayout;

	switchLayout->addWidget(whenLabel);
	switchLayout->addWidget(audioSources);
	switchLayout->addWidget(aboveLabel);
	switchLayout->addWidget(audioVolumeThreshold);
	switchLayout->addWidget(switchLabel);
	switchLayout->addWidget(audioScenes);
	switchLayout->addWidget(usingLabel);
	switchLayout->addWidget(audioTransitions);
	switchLayout->addStretch();

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);
	mainLayout->addWidget(volMeter);

	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

void AudioSwitchWidget::SceneChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->scene = GetWeakSourceByQString(text);
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

AudioSwitch *AudioSwitchWidget::getSwitchData()
{
	return switchData;
}

void AudioSwitchWidget::TransitionChanged(const QString &text)
{
	if (loading || !switchData)
		return;
	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->transition = GetWeakTransitionByQString(text);
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
