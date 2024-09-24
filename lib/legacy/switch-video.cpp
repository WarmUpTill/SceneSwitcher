#include "advanced-scene-switcher.hpp"
#include "layout-helpers.hpp"
#include "source-helpers.hpp"
#include "selection-helpers.hpp"
#include "switcher-data.hpp"
#include "ui-helpers.hpp"
#include "utility.hpp"

#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <thread>

namespace advss {

bool VideoSwitch::pause = false;
static QObject *addPulse = nullptr;

void AdvSceneSwitcher::on_videoAdd_clicked()
{
	std::lock_guard<std::mutex> lock(switcher->m);
	switcher->videoSwitches.emplace_back();

	VideoSwitchWidget *sw =
		new VideoSwitchWidget(this, &switcher->videoSwitches.back());

	listAddClicked(ui->videoSwitches, sw, &addPulse);

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
	auto screenshotData = std::make_unique<Screenshot>(source);
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
	if (!screenshotData->IsDone()) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

	if (!screenshotData->IsDone()) {
		DisplayMessage("Failed to get screenshot of source!");
		return;
	}

	screenshotData->GetImage().save(file.fileName());
	sw->SetFilePath(file.fileName());
}

bool SwitcherData::checkVideoSwitch(OBSWeakSource &scene,
				    OBSWeakSource &transition)
{
	if (VideoSwitch::pause) {
		return false;
	}

	bool match = false;
	for (auto &s : videoSwitches) {
		bool matched = s.checkMatch();
		if (!match && matched) {
			match = true;
			scene = s.getScene();
			transition = s.transition;
			if (VerboseLoggingEnabled()) {
				s.logMatch();
			}
		}
	}
	return match;
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
	videoSwitches.clear();

	obs_data_array_t *videoArray = obs_data_get_array(obj, "videoSwitches");
	size_t count = obs_data_array_count(videoArray);

	for (size_t i = 0; i < count; i++) {
		obs_data_t *array_obj = obs_data_array_item(videoArray, i);

		videoSwitches.emplace_back();
		videoSwitches.back().load(array_obj);

		obs_data_release(array_obj);
	}
	obs_data_array_release(videoArray);
}

void AdvSceneSwitcher::SetupVideoTab()
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
		if (!switcher->disableHints) {
			addPulse = HighlightWidget(ui->videoAdd,
						   QColor(Qt::green));
		}
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

bool requiresFileInput(videoSwitchType t)
{
	return t == videoSwitchType::MATCH || t == videoSwitchType::DIFFER;
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

	if (requiresFileInput(condition)) {
		(void)loadImageFromFile();
	}
}

void VideoSwitch::getScreenshot()
{
	auto source = obs_weak_source_get_source(videoSource);
	screenshotData = std::make_unique<Screenshot>(source);
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
		matchImage.convertToFormat(QImage::Format::Format_RGBA8888);
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

	if (!screenshotData) {
		getScreenshot();
		return match;
	}
	if (!screenshotData->IsDone()) {
		getScreenshot();
		return match;
	}

	bool conditionMatch = false;

	switch (condition) {
	case videoSwitchType::MATCH:
		conditionMatch = screenshotData->GetImage() == matchImage;
		break;
	case videoSwitchType::DIFFER:
		conditionMatch = screenshotData->GetImage() != matchImage;
		break;
	case videoSwitchType::HAS_NOT_CHANGED:
		conditionMatch = screenshotData->GetImage() == matchImage;
		break;
	case videoSwitchType::HAS_CHANGED:
		conditionMatch = screenshotData->GetImage() != matchImage;
		break;
	default:
		break;
	}

	if (conditionMatch) {
		currentMatchDuration +=
			std::chrono::duration_cast<std::chrono::milliseconds>(
				screenshotData->GetScreenshotTime() -
				previousTime);
	} else {
		currentMatchDuration = {};
	}

	bool durationMatch = currentMatchDuration.count() >= duration * 1000;

	if (conditionMatch && durationMatch) {
		match = true;
	}

	if (!requiresFileInput(condition)) {
		matchImage = screenshotData->GetImage();
	}
	previousTime = screenshotData->GetScreenshotTime();

	screenshotData.reset(nullptr);

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
	list->addItem(obs_module_text(
		"AdvSceneSwitcher.videoTab.condition.hasChanged"));
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

	PopulateVideoSelection(videoSources);
	populateConditionSelection(condition);

	if (s) {
		videoSources->setCurrentText(
			GetWeakSourceName(s->videoSource).c_str());
		condition->setCurrentIndex(static_cast<int>(s->condition));
		duration->setValue(s->duration);
		filePath->setText(QString::fromStdString(s->file));
		ignoreInactiveSource->setChecked(s->ignoreInactiveSource);

		if (!requiresFileInput(s->condition)) {
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
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.videoTab.entry"),
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
	if (!switchData || !requiresFileInput(switchData->condition)) {
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

	if (requiresFileInput(switchData->condition)) {
		filePath->show();
		browseButton->show();
	} else {
		filePath->hide();
		browseButton->hide();
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

} // namespace advss
