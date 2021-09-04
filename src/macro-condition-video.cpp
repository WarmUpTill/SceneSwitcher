#include "headers/macro-condition-edit.hpp"
#include "headers/macro-condition-video.hpp"
#include "headers/utility.hpp"
#include "headers/advanced-scene-switcher.hpp"

#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <QMessageBox>
#include <opencv2/opencv.hpp>

const std::string MacroConditionVideo::id = "video";

bool MacroConditionVideo::_registered = MacroConditionFactory::Register(
	MacroConditionVideo::id,
	{MacroConditionVideo::Create, MacroConditionVideoEdit::Create,
	 "AdvSceneSwitcher.condition.video"});

static std::map<VideoCondition, std::string> conditionTypes = {
	{VideoCondition::MATCH,
	 "AdvSceneSwitcher.condition.video.condition.match"},
	{VideoCondition::DIFFER,
	 "AdvSceneSwitcher.condition.video.condition.differ"},
	{VideoCondition::HAS_NOT_CHANGED,
	 "AdvSceneSwitcher.condition.video.condition.hasNotChanged"},
	{VideoCondition::HAS_CHANGED,
	 "AdvSceneSwitcher.condition.video.condition.hasChanged"},
	{VideoCondition::NO_IMAGE,
	 "AdvSceneSwitcher.condition.video.condition.noImage"},
	{VideoCondition::PATTERN,
	 "AdvSceneSwitcher.condition.video.condition.pattern"},
};

bool requiresFileInput(VideoCondition t)
{
	return t == VideoCondition::MATCH || t == VideoCondition::DIFFER ||
	       t == VideoCondition::PATTERN;
}

bool MacroConditionVideo::CheckCondition()
{
	bool match = false;

	if (_screenshotData) {
		if (_screenshotData->done) {
			match = Compare();

			if (!requiresFileInput(_condition)) {
				_matchImage = std::move(_screenshotData->image);
			}
			_screenshotData.reset(nullptr);
		}
	}

	GetScreenshot();
	return match;
}

bool MacroConditionVideo::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	obs_data_set_string(obj, "videoSource",
			    GetWeakSourceName(_videoSource).c_str());
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "filePath", _file.c_str());
	obs_data_set_double(obj, "threshold", _threshold);
	return true;
}

bool MacroConditionVideo::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *videoSourceName = obs_data_get_string(obj, "videoSource");
	_videoSource = GetWeakSourceByName(videoSourceName);
	_condition =
		static_cast<VideoCondition>(obs_data_get_int(obj, "condition"));
	_file = obs_data_get_string(obj, "filePath");
	_threshold = obs_data_get_double(obj, "threshold");

	if (requiresFileInput(_condition)) {
		(void)LoadImageFromFile();
	}
	return true;
}

std::string MacroConditionVideo::GetShortDesc()
{
	if (_videoSource) {
		return GetWeakSourceName(_videoSource);
	}
	return "";
}

void MacroConditionVideo::GetScreenshot()
{
	auto source = obs_weak_source_get_source(_videoSource);
	_screenshotData = std::make_unique<AdvSSScreenshotObj>(source);
	obs_source_release(source);
}

bool MacroConditionVideo::LoadImageFromFile()
{
	if (!_matchImage.load(QString::fromStdString(_file))) {
		blog(LOG_WARNING, "Cannot load image data from file '%s'",
		     _file.c_str());
		return false;
	}
	_matchImage =
		_matchImage.convertToFormat(QImage::Format::Format_RGBX8888);
	return true;
}

// Assumption is that QImage uses Format_RGBX8888.
// Conversion from: https://github.com/dbzhang800/QtOpenCV
cv::Mat QImageToMat(const QImage &img)
{
	if (img.isNull()) {
		return cv::Mat();
	}
	return cv::Mat(img.height(), img.width(), CV_8UC(img.depth() / 8),
		       (uchar *)img.bits(), img.bytesPerLine());
}

QImage MatToQImage(const cv::Mat &mat)
{
	if (mat.empty()) {
		return QImage();
	}
	return QImage(mat.data, mat.cols, mat.rows, mat.step,
		      QImage::Format::Format_RGBX8888);
}

void matchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::Mat &result)
{
	if (img.isNull() || pattern.isNull()) {
		return;
	}
	if (img.height() < pattern.height() || img.width() < pattern.width()) {
		return;
	}

	auto i = QImageToMat(img);
	auto p = QImageToMat(pattern);

	cv::matchTemplate(i, p, result, cv::TM_CCOEFF_NORMED);
	cv::threshold(result, result, threshold, 0, cv::THRESH_TOZERO);
}

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	matchPattern(_screenshotData->image, _matchImage, _threshold, result);
	return countNonZero(result) > 0;
}

bool MacroConditionVideo::Compare()
{
	switch (_condition) {
	case VideoCondition::MATCH:
		return _screenshotData->image == _matchImage;
	case VideoCondition::DIFFER:
		return _screenshotData->image != _matchImage;
	case VideoCondition::HAS_CHANGED:
		return _screenshotData->image != _matchImage;
	case VideoCondition::HAS_NOT_CHANGED:
		return _screenshotData->image == _matchImage;
	case VideoCondition::NO_IMAGE:
		return _screenshotData->image.isNull();
	case VideoCondition::PATTERN:
		return ScreenshotContainsPattern();
	default:
		break;
	}
	return false;
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

ThresholdSlider::ThresholdSlider(QWidget *parent) : QWidget(parent)
{
	_slider = new QSlider();
	_slider->setOrientation(Qt::Horizontal);
	_slider->setRange(0, _scale);
	_value = new QLabel();
	QString labelText =
		obs_module_text("AdvSceneSwitcher.condition.video.threshold") +
		QString("0.");
	for (int i = 0; i < _precision; i++) {
		labelText.append(QString("0"));
	}
	_value->setText(labelText);
	connect(_slider, SIGNAL(valueChanged(int)), this,
		SLOT(NotifyValueChanged(int)));
	QVBoxLayout *mainLayout = new QVBoxLayout();
	QHBoxLayout *sliderLayout = new QHBoxLayout();
	sliderLayout->addWidget(_value);
	sliderLayout->addWidget(_slider);
	mainLayout->addLayout(sliderLayout);
	mainLayout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.thresholdDescription")));
	mainLayout->setContentsMargins(0, 0, 0, 0);
	setLayout(mainLayout);
}

void ThresholdSlider::SetDoubleValue(double value)
{
	_slider->setValue(value * _scale);
	SetDoubleValueText(value);
}

void ThresholdSlider::NotifyValueChanged(int value)
{
	double doubleValue = value / _scale;
	SetDoubleValueText(doubleValue);
	emit DoubleValueChanged(doubleValue);
}

void ThresholdSlider::SetDoubleValueText(double value)
{
	QString labelText = _value->text();
	labelText.chop(_precision + 2); // 2 for the part left of the "."
	labelText.append(QString::number(value, 'f', _precision));
	_value->setText(labelText);
}

MacroConditionVideoEdit::MacroConditionVideoEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVideo> entryData)
	: QWidget(parent)
{
	_videoSelection = new QComboBox();
	_condition = new QComboBox();
	_filePath = new QLineEdit();
	_browseButton =
		new QPushButton(obs_module_text("AdvSceneSwitcher.browse"));
	_threshold = new ThresholdSlider();
	_showMatch = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.video.showMatch"));

	_filePath->setFixedWidth(100);
	_browseButton->setStyleSheet("border:1px solid gray;");

	QWidget::connect(_videoSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_filePath, SIGNAL(editingFinished()), this,
			 SLOT(FilePathChanged()));
	QWidget::connect(_browseButton, SIGNAL(clicked()), this,
			 SLOT(BrowseButtonClicked()));
	QWidget::connect(_threshold, SIGNAL(DoubleValueChanged(double)), this,
			 SLOT(ThresholdChanged(double)));
	QWidget::connect(_showMatch, SIGNAL(clicked()), this,
			 SLOT(ShowMatchClicked()));

	populateVideoSelection(_videoSelection);
	populateConditionSelection(_condition);

	QHBoxLayout *entryLayout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", _videoSelection},
		{"{{condition}}", _condition},
		{"{{filePath}}", _filePath},
		{"{{browseButton}}", _browseButton},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     entryLayout, widgetPlaceholders);
	QHBoxLayout *showMatchLayout = new QHBoxLayout;
	showMatchLayout->addWidget(_showMatch);
	showMatchLayout->addStretch();
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLayout);
	mainLayout->addWidget(_threshold);
	mainLayout->addLayout(showMatchLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;
}

void MacroConditionVideoEdit::UpdatePreviewTooltip()
{
	if (!_entryData) {
		return;
	}

	if (!requiresFileInput(_entryData->_condition)) {
		this->setToolTip("");
		return;
	}

	QImage preview = _entryData->GetMatchImage().scaled(
		{300, 300}, Qt::KeepAspectRatio);

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

void MacroConditionVideoEdit::SetFilePath(const QString &text)
{
	_filePath->setText(text);
	FilePathChanged();
}

void MacroConditionVideoEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_videoSource = GetWeakSourceByQString(text);
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

bool needsThreshold(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN;
}

void MacroConditionVideoEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_condition = static_cast<VideoCondition>(cond);
	SetWidgetVisibility();

	// Reload image data to avoid incorrect matches.
	//
	// Condition type HAS_NOT_CHANGED will use matchImage to store previous
	// frame of video source, which will differ from the image stored at
	// specified file location.
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void MacroConditionVideoEdit::FilePathChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_file = _filePath->text().toUtf8().constData();
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void MacroConditionVideoEdit::BrowseButtonClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	QString path;
	bool useExistingFile = false;
	// Ask whether to create screenshot or to select existing file
	if (_entryData->_videoSource) {
		QMessageBox msgBox(
			QMessageBox::Question,
			obs_module_text("AdvSceneSwitcher.windowTitle"),
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction"),
			QMessageBox::Yes | QMessageBox::No);
		msgBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint |
				      Qt::CustomizeWindowHint);
		msgBox.setButtonText(
			QMessageBox::Yes,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.file"));
		msgBox.setButtonText(
			QMessageBox::No,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.screenshot"));
		useExistingFile = msgBox.exec() == QMessageBox::Yes;
	}

	if (useExistingFile) {
		path = QFileDialog::getOpenFileName(this);
		if (path.isEmpty()) {
			return;
		}

	} else {
		auto source =
			obs_weak_source_get_source(_entryData->_videoSource);
		auto screenshot = std::make_unique<AdvSSScreenshotObj>(source);
		obs_source_release(source);

		path = QFileDialog::getSaveFileName(this);
		if (path.isEmpty()) {
			return;
		}
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return;
		}
		if (!screenshot->done) { // Screenshot usually completed by now
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		if (!screenshot->done) {
			DisplayMessage(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotFail"));
			return;
		}

		screenshot->image.save(path);
	}
	SetFilePath(path);
}

void MacroConditionVideoEdit::ThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(switcher->m);
	_entryData->_threshold = value;
}

QImage markPatterns(cv::Mat &matchResult, QImage &image, QImage &pattern)
{
	auto matchImg = QImageToMat(image);
	for (int row = 0; row < matchResult.rows - 1; row++) {
		for (int col = 0; col < matchResult.cols - 1; col++) {
			if (matchResult.at<float>(row, col) != 0.0) {
				rectangle(matchImg, {col, row},
					  cv::Point(col + pattern.width(),
						    row + pattern.height()),
					  cv::Scalar(255, 0, 0, 0), 2, 8, 0);
			}
		}
	}
	return MatToQImage(matchImg);
}

void MacroConditionVideoEdit::ShowMatchClicked()
{
	auto source = obs_weak_source_get_source(_entryData->_videoSource);
	auto screenshot = std::make_unique<AdvSSScreenshotObj>(source);
	obs_source_release(source);
	if (!screenshot->done) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	if (!screenshot->done) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		return;
	}

	cv::Mat result;
	QImage pattern = _entryData->GetMatchImage();
	matchPattern(screenshot->image, pattern, _entryData->_threshold,
		     result);

	if (countNonZero(result) == 0) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.patternMatchFail"));
		return;
	}

	auto markedIamge = markPatterns(result, screenshot->image, pattern);

	QLabel *label = new QLabel;
	label->setPixmap(QPixmap::fromImage(markedIamge));
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(label);
	QDialog dialog;
	dialog.setLayout(layout);
	dialog.setWindowTitle("Advanced Scene Switcher");
	dialog.exec();
}

void MacroConditionVideoEdit::SetWidgetVisibility()
{
	bool showFileWidgets = requiresFileInput(_entryData->_condition);
	_browseButton->setVisible(showFileWidgets);
	_filePath->setVisible(showFileWidgets);
	_threshold->setVisible(needsThreshold(_entryData->_condition));
	_showMatch->setVisible(_entryData->_condition ==
			       VideoCondition::PATTERN);
	adjustSize();
}

void MacroConditionVideoEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_videoSelection->setCurrentText(
		GetWeakSourceName(_entryData->_videoSource).c_str());
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_filePath->setText(QString::fromStdString(_entryData->_file));
	_threshold->SetDoubleValue(_entryData->_threshold);
	SetWidgetVisibility();
}
