#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool VideoSwitch::pause = false;
static QMetaObject::Connection addPulse;

void AdvSceneSwitcher::on_videoAdd_clicked()
{
	//	std::lock_guard<std::mutex> lock(switcher->m);
	//	switcher->audioSwitches.emplace_back();
	//
	//	AudioSwitchWidget *sw =
	//		new AudioSwitchWidget(this, &switcher->audioSwitches.back());
	//
	//	listAddClicked(ui->audioSwitches, sw, ui->audioAdd, &addPulse);
	//
	//	ui->audioHelp->setVisible(false);
}

void AdvSceneSwitcher::on_videoRemove_clicked()
{
	//	QListWidgetItem *item = ui->audioSwitches->currentItem();
	//	if (!item) {
	//		return;
	//	}
	//
	//	{
	//		std::lock_guard<std::mutex> lock(switcher->m);
	//		int idx = ui->audioSwitches->currentRow();
	//		auto &switches = switcher->audioSwitches;
	//		switches.erase(switches.begin() + idx);
	//	}
	//
	//	delete item;
}

void AdvSceneSwitcher::on_videoUp_clicked()
{
	//	int index = ui->audioSwitches->currentRow();
	//	if (!listMoveUp(ui->audioSwitches)) {
	//		return;
	//	}
	//
	//	AudioSwitchWidget *s1 =
	//		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
	//			ui->audioSwitches->item(index));
	//	AudioSwitchWidget *s2 =
	//		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
	//			ui->audioSwitches->item(index - 1));
	//	AudioSwitchWidget::swapSwitchData(s1, s2);
	//
	//	std::lock_guard<std::mutex> lock(switcher->m);
	//
	//	std::swap(switcher->audioSwitches[index],
	//		  switcher->audioSwitches[index - 1]);
}

void AdvSceneSwitcher::on_videoDown_clicked()
{
	//	int index = ui->audioSwitches->currentRow();
	//
	//	if (!listMoveDown(ui->audioSwitches)) {
	//		return;
	//	}
	//
	//	AudioSwitchWidget *s1 =
	//		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
	//			ui->audioSwitches->item(index));
	//	AudioSwitchWidget *s2 =
	//		(AudioSwitchWidget *)ui->audioSwitches->itemWidget(
	//			ui->audioSwitches->item(index + 1));
	//	AudioSwitchWidget::swapSwitchData(s1, s2);
	//
	//	std::lock_guard<std::mutex> lock(switcher->m);
	//
	//	std::swap(switcher->audioSwitches[index],
	//		  switcher->audioSwitches[index + 1]);
}

void AdvSceneSwitcher::on_getScreenshot_clicked() {}

void SwitcherData::checkVideoSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (VideoSwitch::pause) {
		return;
	}
}

void SwitcherData::saveVideoSwitches(obs_data_t *obj)
{
	//	obs_data_array_t *audioArray = obs_data_array_create();
	//	for (AudioSwitch &s : switcher->audioSwitches) {
	//		obs_data_t *array_obj = obs_data_create();
	//
	//		s.save(array_obj);
	//		obs_data_array_push_back(audioArray, array_obj);
	//
	//		obs_data_release(array_obj);
	//	}
	//	obs_data_set_array(obj, "audioSwitches", audioArray);
	//	obs_data_array_release(audioArray);
	//
	//	audioFallback.save(obj);
}

void SwitcherData::loadVideoSwitches(obs_data_t *obj)
{
	//	switcher->audioSwitches.clear();
	//
	//	obs_data_array_t *audioArray = obs_data_get_array(obj, "audioSwitches");
	//	size_t count = obs_data_array_count(audioArray);
	//
	//	for (size_t i = 0; i < count; i++) {
	//		obs_data_t *array_obj = obs_data_array_item(audioArray, i);
	//
	//		switcher->audioSwitches.emplace_back();
	//		audioSwitches.back().load(array_obj);
	//
	//		obs_data_release(array_obj);
	//	}
	//	obs_data_array_release(audioArray);
	//
	//	audioFallback.load(obj);
}

void AdvSceneSwitcher::setupVideoTab()
{
	//	for (auto &s : switcher->audioSwitches) {
	//		QListWidgetItem *item;
	//		item = new QListWidgetItem(ui->audioSwitches);
	//		ui->audioSwitches->addItem(item);
	//		AudioSwitchWidget *sw = new AudioSwitchWidget(this, &s);
	//		item->setSizeHint(sw->minimumSizeHint());
	//		ui->audioSwitches->setItemWidget(item, sw);
	//	}
	//
	//	if (switcher->audioSwitches.size() == 0) {
	//		addPulse = PulseWidget(ui->audioAdd, QColor(Qt::green));
	//		ui->audioHelp->setVisible(true);
	//	} else {
	//		ui->audioHelp->setVisible(false);
	//	}
	//
	//	AudioSwitchFallbackWidget *fb =
	//		new AudioSwitchFallbackWidget(this, &switcher->audioFallback);
	//	ui->audioFallbackLayout->addWidget(fb);
	//	ui->audioFallback->setChecked(switcher->audioFallback.enable);
}

bool VideoSwitch::initialized()
{
	return SceneSwitcherEntry::initialized() && videoSource;
}

bool VideoSwitch::valid()
{
	return !initialized() ||
	       (SceneSwitcherEntry::valid() && WeakSourceValid(videoSource));
}

void VideoSwitch::save(obs_data_t *obj)
{
	SceneSwitcherEntry::save(obj);

	obs_data_set_string(obj, "videoSource",
			    GetWeakSourceName(videoSource).c_str());
}

void VideoSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	const char *videoSourceName = obs_data_get_string(obj, "videoSource");
	videoSource = GetWeakSourceByName(videoSourceName);
}

void swap(VideoSwitch &first, VideoSwitch &second)
{
	std::swap(first.targetType, second.targetType);
	std::swap(first.group, second.group);
	std::swap(first.scene, second.scene);
	std::swap(first.transition, second.transition);
	std::swap(first.usePreviousScene, second.usePreviousScene);
	std::swap(first.videoSource, second.videoSource);
}

VideoSwitchWidget::VideoSwitchWidget(QWidget *parent, VideoSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	videoSources = new QComboBox();

	obs_source_t *source = nullptr;

	QWidget::connect(videoSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));

	AdvSceneSwitcher::populateAudioSelection(videoSources);

	if (s) {
		videoSources->setCurrentText(
			GetWeakSourceName(s->videoSource).c_str());
	}

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", videoSources},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.videoTab.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);
	setLayout(mainLayout);

	switchData = s;

	loading = false;
}

VideoSwitch *VideoSwitchWidget::getSwitchData()
{
	return switchData;
}

void VideoSwitchWidget::setSwitchData(VideoSwitch *s)
{
	switchData = s;
}

void VideoSwitchWidget::swapSwitchData(VideoSwitchWidget *s1,
				       VideoSwitchWidget *s2)
{
	SwitchWidget::swapSwitchData(s1, s2);

	VideoSwitch *t = s1->getSwitchData();
	s1->setSwitchData(s2->getSwitchData());
	s2->setSwitchData(t);
}

void VideoSwitchWidget::SourceChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->videoSource = GetWeakSourceByQString(text);
}
