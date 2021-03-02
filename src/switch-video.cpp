#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <thread>

#include "headers/advanced-scene-switcher.hpp"
#include "headers/utility.hpp"

bool VideoSwitch::pause = false;
static QMetaObject::Connection addPulse;

static void ScreenshotTick(void *param, float);

AdvSSScreenshotObj::AdvSSScreenshotObj(obs_source_t *source)
	: weakSource(OBSGetWeakRef(source))
{
	obs_add_tick_callback(ScreenshotTick, this);
}

AdvSSScreenshotObj::~AdvSSScreenshotObj()
{
	obs_enter_graphics();
	gs_stagesurface_destroy(stagesurf);
	gs_texrender_destroy(texrender);
	obs_leave_graphics();

	obs_remove_tick_callback(ScreenshotTick, this);
}

void AdvSSScreenshotObj::Screenshot()
{
	OBSSource source = OBSGetStrongRef(weakSource);

	if (source) {
		cx = obs_source_get_base_width(source);
		cy = obs_source_get_base_height(source);
	} else {
		obs_video_info ovi;
		obs_get_video_info(&ovi);
		cx = ovi.base_width;
		cy = ovi.base_height;
	}

	if (!cx || !cy) {
		blog(LOG_WARNING, "Cannot screenshot, invalid target size");
		obs_remove_tick_callback(ScreenshotTick, this);
		done = true;
		return;
	}

	texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	stagesurf = gs_stagesurface_create(cx, cy, GS_RGBA);

	gs_texrender_reset(texrender);
	if (gs_texrender_begin(texrender, cx, cy)) {
		vec4 zero;
		vec4_zero(&zero);

		gs_clear(GS_CLEAR_COLOR, &zero, 0.0f, 0);
		gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);

		if (source) {
			obs_source_inc_showing(source);
			obs_source_video_render(source);
			obs_source_dec_showing(source);
		} else {
			obs_render_main_texture();
		}

		gs_blend_state_pop();
		gs_texrender_end(texrender);
	}
}

void AdvSSScreenshotObj::Download()
{
	gs_stage_texture(stagesurf, gs_texrender_get_texture(texrender));
}

void AdvSSScreenshotObj::Copy()
{
	uint8_t *videoData = nullptr;
	uint32_t videoLinesize = 0;

	image = QImage(cx, cy, QImage::Format::Format_RGBX8888);

	if (gs_stagesurface_map(stagesurf, &videoData, &videoLinesize)) {
		int linesize = image.bytesPerLine();
		for (int y = 0; y < (int)cy; y++)
			memcpy(image.scanLine(y),
			       videoData + (y * videoLinesize), linesize);

		gs_stagesurface_unmap(stagesurf);
	}
}

void AdvSSScreenshotObj::MarkDone()
{
	time = std::chrono::high_resolution_clock::now();
	done = true;
}

#define STAGE_SCREENSHOT 0
#define STAGE_DOWNLOAD 1
#define STAGE_COPY_AND_SAVE 2
#define STAGE_FINISH 3

static void ScreenshotTick(void *param, float)
{
	AdvSSScreenshotObj *data =
		reinterpret_cast<AdvSSScreenshotObj *>(param);

	if (data->stage == STAGE_FINISH) {
		return;
	}

	obs_enter_graphics();

	switch (data->stage) {
	case STAGE_SCREENSHOT:
		data->Screenshot();
		break;
	case STAGE_DOWNLOAD:
		data->Download();
		break;
	case STAGE_COPY_AND_SAVE:
		data->Copy();
		data->MarkDone();

		obs_remove_tick_callback(ScreenshotTick, data);
		break;
	}

	obs_leave_graphics();

	data->stage++;
}

void AdvSceneSwitcher::on_videoAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->videoSwitches.emplace_back();

	VideoSwitchWidget *sw =
		new VideoSwitchWidget(this, &switcher->videoSwitches.back());

	listAddClicked(ui->videoSwitches, sw, ui->videoAdd, &addPulse);

	ui->videoHelp->setVisible(false);
}

void AdvSceneSwitcher::on_videoRemove_clicked()
{
	QListWidgetItem *item = ui->videoSwitches->currentItem();
	if (!item) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(switcher->m);
		int idx = ui->videoSwitches->currentRow();
		auto &switches = switcher->videoSwitches;
		switches.erase(switches.begin() + idx);
	}

	delete item;
}

void AdvSceneSwitcher::on_videoUp_clicked()
{
	int index = ui->videoSwitches->currentRow();
	if (!listMoveUp(ui->videoSwitches)) {
		return;
	}

	VideoSwitchWidget *s1 =
		(VideoSwitchWidget *)ui->videoSwitches->itemWidget(
			ui->videoSwitches->item(index));
	VideoSwitchWidget *s2 =
		(VideoSwitchWidget *)ui->videoSwitches->itemWidget(
			ui->videoSwitches->item(index - 1));
	VideoSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->videoSwitches[index],
		  switcher->videoSwitches[index - 1]);
}

void AdvSceneSwitcher::on_videoDown_clicked()
{
	int index = ui->videoSwitches->currentRow();

	if (!listMoveDown(ui->videoSwitches)) {
		return;
	}

	VideoSwitchWidget *s1 =
		(VideoSwitchWidget *)ui->videoSwitches->itemWidget(
			ui->videoSwitches->item(index));
	VideoSwitchWidget *s2 =
		(VideoSwitchWidget *)ui->videoSwitches->itemWidget(
			ui->videoSwitches->item(index + 1));
	VideoSwitchWidget::swapSwitchData(s1, s2);

	std::lock_guard<std::mutex> lock(switcher->m);

	std::swap(switcher->videoSwitches[index],
		  switcher->videoSwitches[index + 1]);
}

void AdvSceneSwitcher::on_getScreenshot_clicked()
{
	QListWidgetItem *item = ui->videoSwitches->currentItem();

	if (!item) {
		return;
	}

	VideoSwitchWidget *sw =
		(VideoSwitchWidget *)ui->videoSwitches->itemWidget(item);
	auto s = sw->getSwitchData();
	if (!s || !s->videoSource) {
		return;
	}

	auto source = obs_weak_source_get_source(s->videoSource);
	auto screenshotData = std::make_unique<AdvSSScreenshotObj>(source);
	obs_source_release(source);

	QString filePath = QFileDialog::getSaveFileName(this);
	if (filePath.isEmpty()) {
		return;
	}

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		return;
	}

	// During selection of the save path enough time should usually have
	// passed already
	// Add this just in case ...
	if (!screenshotData->done) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	if (!screenshotData->done) {
		DisplayMessage("Failed to get screenshot of source!");
		return;
	}

	screenshotData->image.save(file.fileName());
	sw->SetFilePath(file.fileName());
}

void SwitcherData::checkVideoSwitch(bool &match, OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (VideoSwitch::pause) {
		return;
	}

	for (auto &s : videoSwitches) {
		bool matched = s.checkMatch();
		if (!match && matched) {
			match = true;
			scene = s.getScene();
			transition = s.transition;
			if (verbose) {
				s.logMatch();
			}
		}
	}
}

void SwitcherData::saveVideoSwitches(obs_data_t *obj)
{
	obs_data_array_t *videoArray = obs_data_array_create();
	for (VideoSwitch &s : videoSwitches) {
		obs_data_t *array_obj = obs_data_create();

		s.save(array_obj);
		obs_data_array_push_back(videoArray, array_obj);

		obs_data_release(array_obj);
	}
	obs_data_set_array(obj, "videoSwitches", videoArray);
	obs_data_array_release(videoArray);
}

void SwitcherData::loadVideoSwitches(obs_data_t *obj)
{
	switcher->videoSwitches.clear();

	obs_data_array_t *videoArray = obs_data_get_array(obj, "videoSwitches");
	size_t count = obs_data_array_count(videoArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(videoArray, i);

		switcher->videoSwitches.emplace_back();
		videoSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(videoArray);
}

void AdvSceneSwitcher::setupVideoTab()
{
	for (auto &s : switcher->videoSwitches) {
		QListWidgetItem *item;
		item = new QListWidgetItem(ui->videoSwitches);
		ui->videoSwitches->addItem(item);
		VideoSwitchWidget *sw = new VideoSwitchWidget(this, &s);
		item->setSizeHint(sw->minimumSizeHint());
		ui->videoSwitches->setItemWidget(item, sw);
	}

	if (switcher->videoSwitches.size() == 0) {
		addPulse = PulseWidget(ui->videoAdd, QColor(Qt::green));
		ui->videoHelp->setVisible(true);
	} else {
		ui->videoHelp->setVisible(false);
	}

	ui->getScreenshot->setToolTip(
		obs_module_text("AdvSceneSwitcher.videoTab.getScreenshotHelp"));
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
	obs_data_set_int(obj, "condition", static_cast<int>(condition));
	obs_data_set_double(obj, "duration", duration);
	obs_data_set_string(obj, "filePath", file.c_str());
	obs_data_set_bool(obj, "ignoreInactiveSource", ignoreInactiveSource);
}

void VideoSwitch::load(obs_data_t *obj)
{
	SceneSwitcherEntry::load(obj);

	const char *videoSourceName = obs_data_get_string(obj, "videoSource");
	videoSource = GetWeakSourceByName(videoSourceName);
	condition = static_cast<videoSwitchType>(
		obs_data_get_int(obj, "condition"));
	duration = obs_data_get_double(obj, "duration");
	file = obs_data_get_string(obj, "filePath");
	ignoreInactiveSource = obs_data_get_bool(obj, "ignoreInactiveSource");

	if (condition != videoSwitchType::HAS_NOT_CHANGED) {
		(void)loadImageFromFile();
	}
}

void VideoSwitch::getScreenshot()
{
	auto source = obs_weak_source_get_source(videoSource);
	screenshotData = std::make_unique<AdvSSScreenshotObj>(source);
	obs_source_release(source);
}

bool VideoSwitch::loadImageFromFile()
{
	if (!matchImage.load(QString::fromStdString(file))) {
		blog(LOG_WARNING, "Cannot load image data from file '%s'",
		     file.c_str());
		return false;
	}
	matchImage =
		matchImage.convertToFormat(QImage::Format::Format_RGBX8888);
	return true;
}

bool VideoSwitch::checkMatch()
{
	if (ignoreInactiveSource) {
		obs_source_t *vs = obs_weak_source_get_source(videoSource);
		bool videoActive = obs_source_active(vs);
		obs_source_release(vs);

		if (!videoActive) {
			screenshotData.reset(nullptr);
			return false;
		}
	}

	bool match = false;

	if (screenshotData) {
		if (screenshotData->done) {
			bool conditionMatch = false;

			switch (condition) {
			case videoSwitchType::MATCH:
				conditionMatch = screenshotData->image ==
						 matchImage;
				break;
			case videoSwitchType::DIFFER:
				conditionMatch = screenshotData->image !=
						 matchImage;
				break;
			case videoSwitchType::HAS_NOT_CHANGED:
				conditionMatch = screenshotData->image ==
						 matchImage;
				break;
			default:
				break;
			}

			if (conditionMatch) {
				currentMatchDuration +=
					std::chrono::duration_cast<
						std::chrono::milliseconds>(
						screenshotData->time -
						previousTime);
			} else {
				currentMatchDuration = {};
			}

			bool durationMatch = currentMatchDuration.count() >=
					     duration * 1000;

			if (conditionMatch && durationMatch) {
				match = true;
			}

			if (condition == videoSwitchType::HAS_NOT_CHANGED) {
				matchImage = std::move(screenshotData->image);
			}
			previousTime = std::move(screenshotData->time);

			screenshotData.reset(nullptr);
		}
	}

	getScreenshot();
	return match;
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

static inline void populateConditionSelection(QComboBox *list)
{
	list->addItem(
		obs_module_text("AdvSceneSwitcher.videoTab.condition.match"));
	list->setItemData(
		0,
		obs_module_text(
			"AdvSceneSwitcher.videoTab.condition.match.tooltip"),
		Qt::ToolTipRole);
	list->addItem(
		obs_module_text("AdvSceneSwitcher.videoTab.condition.differ"));
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.videoTab.condition.hasNotChanged"));
}

VideoSwitchWidget::VideoSwitchWidget(QWidget *parent, VideoSwitch *s)
	: SwitchWidget(parent, s, true, true)
{
	videoSources = new QComboBox();
	condition = new QComboBox();
	duration = new QDoubleSpinBox();
	filePath = new QLineEdit();
	browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	ignoreInactiveSource = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.videoTab.ignoreInactiveSource"));

	filePath->setFixedWidth(100);

	browseButton->setStyleSheet("border:1px solid gray;");

	duration->setMinimum(0.0);
	duration->setMaximum(99.000000);
	duration->setSuffix("s");

	QWidget::connect(videoSources,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(duration, SIGNAL(valueChanged(double)), this,
			 SLOT(DurationChanged(double)));
	QWidget::connect(filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QWidget::connect(ignoreInactiveSource, SIGNAL(stateChanged(int)), this,
			 SLOT(IgnoreInactiveChanged(int)));

	// TODO:
	// Figure out why scene do not work for "match exactly".
	// Until then do not allow selecting scenes
	AdvSceneSwitcher::populateVideoSelection(videoSources, false);
	populateConditionSelection(condition);

	if (s) {
		videoSources->setCurrentText(
			GetWeakSourceName(s->videoSource).c_str());
		condition->setCurrentIndex(static_cast<int>(s->condition));
		duration->setValue(s->duration);
		filePath->setText(QString::fromStdString(s->file));
		ignoreInactiveSource->setChecked(s->ignoreInactiveSource);

		if (s->condition == videoSwitchType::HAS_NOT_CHANGED) {
			filePath->hide();
			browseButton->hide();
		}
	}

	QHBoxLayout *switchLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", videoSources},
		{"{{condition}}", condition},
		{"{{duration}}", duration},
		{"{{filePath}}", filePath},
		{"{{browseButton}}", browseButton},
		{"{{ignoreInactiveSource}}", ignoreInactiveSource},
		{"{{scenes}}", scenes},
		{"{{transitions}}", transitions}};
	placeWidgets(obs_module_text("AdvSceneSwitcher.videoTab.entry"),
		     switchLayout, widgetPlaceholders);

	QVBoxLayout *mainLayout = new QVBoxLayout;

	mainLayout->addLayout(switchLayout);
	setLayout(mainLayout);

	switchData = s;
	UpdatePreviewTooltip();

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

void VideoSwitchWidget::UpdatePreviewTooltip()
{
	if (!switchData ||
	    switchData->condition == videoSwitchType::HAS_NOT_CHANGED) {
		return;
	}

	QImage preview =
		switchData->matchImage.scaled({300, 300}, Qt::KeepAspectRatio);

	QByteArray data;
	QBuffer buffer(&data);
	if (!preview.save(&buffer, "PNG")) {
		return;
	}

	QString html =
		QString("<html><img src='data:image/png;base64, %0'/></html>")
			.arg(QString(data.toBase64()));
	this->setToolTip(html);
}

void VideoSwitchWidget::SetFilePath(const QString &text)
{
	filePath->setText(text);
	FilePathChanged();
}

void VideoSwitchWidget::SourceChanged(const QString &text)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->videoSource = GetWeakSourceByQString(text);
}

void VideoSwitchWidget::ConditionChanged(int cond)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->condition = static_cast<videoSwitchType>(cond);

	if (switchData->condition == videoSwitchType::HAS_NOT_CHANGED) {
		filePath->hide();
		browseButton->hide();
	} else {
		filePath->show();
		browseButton->show();
	}

	// Reload image data to avoid incorrect matches.
	//
	// Condition type HAS_NOT_CHANGED will use matchImage to store previous
	// frame of video source, which will differ from the image stored at
	// specified file location.
	if (switchData->loadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void VideoSwitchWidget::DurationChanged(double dur)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->duration = dur;
}

void VideoSwitchWidget::FilePathChanged()
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->file = filePath->text().toUtf8().constData();
	if (switchData->loadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void VideoSwitchWidget::BrowseButtonClicked()
{
	if (loading || !switchData) {
		return;
	}

	QString path = QFileDialog::getOpenFileName(
		this,
		tr(obs_module_text("AdvSceneSwitcher.fileTab.selectRead")),
		QDir::currentPath(),
		tr(obs_module_text("AdvSceneSwitcher.fileTab.anyFileType")));
	if (path.isEmpty()) {
		return;
	}

	filePath->setText(path);
	FilePathChanged();
}

void VideoSwitchWidget::IgnoreInactiveChanged(int state)
{
	if (loading || !switchData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	switchData->ignoreInactiveSource = state;
}
