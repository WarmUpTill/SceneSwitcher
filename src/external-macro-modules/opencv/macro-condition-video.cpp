#include "macro-condition-video.hpp"

#include <advanced-scene-switcher.hpp>
#include <macro-condition-edit.hpp>
#include <switcher-data-structs.hpp>
#include <utility.hpp>

#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <QMessageBox>

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
	{VideoCondition::OBJECT,
	 "AdvSceneSwitcher.condition.video.condition.object"},
};

cv::CascadeClassifier initObjectCascade(std::string &path)
{
	cv::CascadeClassifier cascade;
	try {
		cascade.load(path);
	} catch (...) {
		blog(LOG_WARNING, "failed to load model data \"%s\"",
		     path.c_str());
	}
	return cascade;
}

bool requiresFileInput(VideoCondition t)
{
	return t == VideoCondition::MATCH || t == VideoCondition::DIFFER ||
	       t == VideoCondition::PATTERN;
}

bool MacroConditionVideo::CheckShouldBeSkipped()
{
	if (_condition != VideoCondition::PATTERN &&
	    _condition != VideoCondition::OBJECT) {
		return false;
	}

	if (_throttleEnabled) {
		if (_runCount <= _throttleCount) {
			_runCount++;
			return true;
		} else {
			_runCount = 0;
		}
	}
	return false;
}

bool MacroConditionVideo::CheckCondition()
{
	bool match = false;

	if (CheckShouldBeSkipped()) {
		return _lastMatchResult;
	}

	if (_screenshotData && _screenshotData->done) {
		match = Compare();
		_lastMatchResult = match;

		if (!requiresFileInput(_condition)) {
			_matchImage = std::move(_screenshotData->image);
		}
		_screenshotData.reset(nullptr);
	} else {
		match = _lastMatchResult;
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
	obs_data_set_bool(obj, "usePatternForChangedCheck",
			  _usePatternForChangedCheck);
	obs_data_set_double(obj, "threshold", _patternThreshold);
	obs_data_set_bool(obj, "useAlphaAsMask", _useAlphaAsMask);
	obs_data_set_string(obj, "modelDataPath", _modelDataPath.c_str());
	obs_data_set_double(obj, "scaleFactor", _scaleFactor);
	obs_data_set_int(obj, "minNeighbors", _minNeighbors);
	obs_data_set_int(obj, "minSizeX", _minSizeX);
	obs_data_set_int(obj, "minSizeY", _minSizeY);
	obs_data_set_int(obj, "maxSizeX", _maxSizeX);
	obs_data_set_int(obj, "maxSizeY", _maxSizeY);
	obs_data_set_bool(obj, "throttleEnabled", _throttleEnabled);
	obs_data_set_int(obj, "throttleCount", _throttleCount);
	return true;
}

bool isScaleFactorValid(double scaleFactor)
{
	return scaleFactor > 1.;
}

bool isMinNeighborsValid(int minNeighbors)
{
	return minNeighbors >= minMinNeighbors &&
	       minNeighbors <= maxMinNeighbors;
}

bool MacroConditionVideo::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	const char *videoSourceName = obs_data_get_string(obj, "videoSource");
	_videoSource = GetWeakSourceByName(videoSourceName);
	_condition =
		static_cast<VideoCondition>(obs_data_get_int(obj, "condition"));
	_file = obs_data_get_string(obj, "filePath");
	_usePatternForChangedCheck =
		obs_data_get_bool(obj, "usePatternForChangedCheck");
	_patternThreshold = obs_data_get_double(obj, "threshold");
	_useAlphaAsMask = obs_data_get_bool(obj, "useAlphaAsMask");
	_modelDataPath = obs_data_get_string(obj, "modelDataPath");
	_scaleFactor = obs_data_get_double(obj, "scaleFactor");
	if (!isScaleFactorValid(_scaleFactor)) {
		_scaleFactor = 1.1;
	}
	_minNeighbors = obs_data_get_int(obj, "minNeighbors");
	if (!isMinNeighborsValid(_minNeighbors)) {
		_minNeighbors = minMinNeighbors;
	}
	_minSizeX = obs_data_get_int(obj, "minSizeX");
	_minSizeY = obs_data_get_int(obj, "minSizeY");
	_maxSizeX = obs_data_get_int(obj, "maxSizeX");
	_maxSizeY = obs_data_get_int(obj, "maxSizeY");
	_throttleEnabled = obs_data_get_bool(obj, "throttleEnabled");
	_throttleCount = obs_data_get_int(obj, "throttleCount");

	if (requiresFileInput(_condition)) {
		(void)LoadImageFromFile();
	}

	if (_condition == VideoCondition::OBJECT) {
		LoadModelData(_modelDataPath);
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

// Assumption is that QImage uses Format_RGBA8888.
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
		      QImage::Format::Format_RGBA8888);
}

PatternMatchData createPatternData(QImage &pattern)
{
	PatternMatchData data;
	if (pattern.isNull()) {
		return data;
	}

	data.rgbaPattern = QImageToMat(pattern);
	std::vector<cv::Mat1b> rgbaChannelsPattern;
	cv::split(data.rgbaPattern, rgbaChannelsPattern);
	std::vector<cv::Mat1b> rgbChanlesPattern(
		rgbaChannelsPattern.begin(), rgbaChannelsPattern.begin() + 3);
	cv::merge(rgbChanlesPattern, data.rgbPattern);
	cv::threshold(rgbaChannelsPattern[3], data.mask, 0, 255,
		      cv::THRESH_BINARY);
	return data;
}

bool MacroConditionVideo::LoadImageFromFile()
{
	if (!_matchImage.load(QString::fromStdString(_file))) {
		blog(LOG_WARNING, "Cannot load image data from file '%s'",
		     _file.c_str());
		return false;
	}

	_matchImage =
		_matchImage.convertToFormat(QImage::Format::Format_RGBA8888);
	_patternData = createPatternData(_matchImage);
	return true;
}

bool MacroConditionVideo::LoadModelData(std::string &path)
{
	_modelDataPath = path;
	_objectCascade = initObjectCascade(path);
	return !_objectCascade.empty();
}

void matchPattern(QImage &img, PatternMatchData &patternData, double threshold,
		  cv::Mat &result, bool useAlphaAsMask = true)
{
	if (img.isNull() || patternData.rgbaPattern.empty()) {
		return;
	}
	if (img.height() < patternData.rgbaPattern.rows ||
	    img.width() < patternData.rgbaPattern.cols) {
		return;
	}

	auto i = QImageToMat(img);

	if (useAlphaAsMask) {
		std::vector<cv::Mat1b> rgbaChannelsImage;
		cv::split(i, rgbaChannelsImage);
		std::vector<cv::Mat1b> rgbChanlesImage(
			rgbaChannelsImage.begin(),
			rgbaChannelsImage.begin() + 3);

		cv::Mat3b rgbImage;
		cv::merge(rgbChanlesImage, rgbImage);

		cv::matchTemplate(rgbImage, patternData.rgbPattern, result,
				  cv::TM_CCORR_NORMED, patternData.mask);
		cv::threshold(result, result, threshold, 0, cv::THRESH_TOZERO);

	} else {
		cv::matchTemplate(i, patternData.rgbaPattern, result,
				  cv::TM_CCOEFF_NORMED);
		cv::threshold(result, result, threshold, 0, cv::THRESH_TOZERO);
	}
}

void matchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::Mat &result, bool useAlphaAsMask)
{
	auto data = createPatternData(pattern);
	matchPattern(img, data, threshold, result, useAlphaAsMask);
}

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	matchPattern(_screenshotData->image, _patternData, _patternThreshold,
		     result, _useAlphaAsMask);
	return countNonZero(result) > 0;
}

bool MacroConditionVideo::OutputChanged()
{
	if (_usePatternForChangedCheck) {
		cv::Mat result;
		_patternData = createPatternData(_matchImage);
		matchPattern(_screenshotData->image, _patternData,
			     _patternThreshold, result, _useAlphaAsMask);
		return countNonZero(result) == 0;
	}
	return _screenshotData->image != _matchImage;
}

std::vector<cv::Rect> matchObject(QImage &img, cv::CascadeClassifier &cascade,
				  double scaleFactor, int minNeighbors,
				  cv::Size minSize, cv::Size maxSize)
{
	if (img.isNull() || cascade.empty()) {
		return {};
	}

	auto i = QImageToMat(img);
	cv::Mat frameGray;
	cv::cvtColor(i, frameGray, cv::COLOR_BGR2GRAY);
	equalizeHist(frameGray, frameGray);
	std::vector<cv::Rect> objects;
	cascade.detectMultiScale(frameGray, objects, scaleFactor, minNeighbors,
				 0, minSize, maxSize);
	return objects;
}

bool MacroConditionVideo::ScreenshotContainsObject()
{
	auto objects = matchObject(_screenshotData->image, _objectCascade,
				   _scaleFactor, _minNeighbors,
				   {_minSizeX, _minSizeY},
				   {_maxSizeX, _maxSizeY});
	return objects.size() > 0;
}

bool MacroConditionVideo::Compare()
{
	switch (_condition) {
	case VideoCondition::MATCH:
		return _screenshotData->image == _matchImage;
	case VideoCondition::DIFFER:
		return _screenshotData->image != _matchImage;
	case VideoCondition::HAS_CHANGED:
		return OutputChanged();
	case VideoCondition::HAS_NOT_CHANGED:
		return !OutputChanged();
	case VideoCondition::NO_IMAGE:
		return _screenshotData->image.isNull();
	case VideoCondition::PATTERN:
		return ScreenshotContainsPattern();
	case VideoCondition::OBJECT:
		return ScreenshotContainsObject();
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

ThresholdSlider::ThresholdSlider(double min, double max, const QString &label,
				 const QString &description, QWidget *parent)
	: QWidget(parent)
{
	_slider = new QSlider();
	_slider->setOrientation(Qt::Horizontal);
	_slider->setRange(min * _scale, max * _scale);
	_value = new QLabel();
	QString labelText = label + QString("0.");
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
	if (!description.isEmpty()) {
		mainLayout->addWidget(new QLabel(description));
	}
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
	: QWidget(parent), _matchDialog(this, entryData.get())
{
	_videoSelection = new QComboBox();
	_condition = new QComboBox();

	_imagePath = new FileSelection();
	_imagePath->Button()->disconnect();
	_usePatternForChangedCheck = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.video.usePatternForChangedCheck"));
	_usePatternForChangedCheck->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.usePatternForChangedCheck.tooltip"));
	_patternThreshold = new ThresholdSlider(
		0., 1.,
		obs_module_text(
			"AdvSceneSwitcher.condition.video.patternThreshold"),
		obs_module_text(
			"AdvSceneSwitcher.condition.video.patternThresholdDescription"));
	_useAlphaAsMask = new QCheckBox(obs_module_text(
		"AdvSceneSwitcher.condition.video.patternThresholdUseAlphaAsMask"));

	_modelDataPath = new FileSelection();
	_objectScaleThreshold = new ThresholdSlider(
		1.1, 5.,
		obs_module_text(
			"AdvSceneSwitcher.condition.video.objectScaleThreshold"),
		obs_module_text(
			"AdvSceneSwitcher.condition.video.objectScaleThresholdDescription"));
	_minNeighbors = new QSpinBox();
	_minNeighbors->setMinimum(minMinNeighbors);
	_minNeighbors->setMaximum(maxMinNeighbors);
	_minNeighborsDescription = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.minNeighborDescription"));
	_minSizeX = new QSpinBox();
	_minSizeY = new QSpinBox();
	_minSizeX->setMaximum(1024);
	_minSizeY->setMaximum(1024);
	_maxSizeX = new QSpinBox();
	_maxSizeY = new QSpinBox();
	_maxSizeX->setMaximum(4096);
	_maxSizeY->setMaximum(4096);

	_throttleEnable = new QCheckBox();
	_throttleCount = new QSpinBox();
	_throttleCount->setMinimum(1 * GetSwitcher()->interval);
	_throttleCount->setMaximum(10 * GetSwitcher()->interval);
	_throttleCount->setSingleStep(GetSwitcher()->interval);
	_showMatch = new QPushButton(
		obs_module_text("AdvSceneSwitcher.condition.video.showMatch"));

	QWidget::connect(_videoSelection,
			 SIGNAL(currentTextChanged(const QString &)), this,
			 SLOT(SourceChanged(const QString &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_imagePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(ImagePathChanged(const QString &)));
	QWidget::connect(_imagePath->Button(), SIGNAL(clicked()), this,
			 SLOT(ImageBrowseButtonClicked()));
	QWidget::connect(_usePatternForChangedCheck, SIGNAL(stateChanged(int)),
			 this, SLOT(UsePatternForChangedCheckChanged(int)));
	QWidget::connect(_patternThreshold, SIGNAL(DoubleValueChanged(double)),
			 this, SLOT(PatternThresholdChanged(double)));
	QWidget::connect(_useAlphaAsMask, SIGNAL(stateChanged(int)), this,
			 SLOT(UseAlphaAsMaskChanged(int)));
	QWidget::connect(_objectScaleThreshold,
			 SIGNAL(DoubleValueChanged(double)), this,
			 SLOT(ObjectScaleThresholdChanged(double)));
	QWidget::connect(_minNeighbors, SIGNAL(valueChanged(int)), this,
			 SLOT(MinNeighborsChanged(int)));
	QWidget::connect(_minSizeX, SIGNAL(valueChanged(int)), this,
			 SLOT(MinSizeXChanged(int)));
	QWidget::connect(_minSizeY, SIGNAL(valueChanged(int)), this,
			 SLOT(MinSizeYChanged(int)));
	QWidget::connect(_maxSizeX, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxSizeXChanged(int)));
	QWidget::connect(_maxSizeY, SIGNAL(valueChanged(int)), this,
			 SLOT(MaxSizeYChanged(int)));
	QWidget::connect(_modelDataPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ModelPathChanged(const QString &)));
	QWidget::connect(_throttleEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(ThrottleEnableChanged(int)));
	QWidget::connect(_throttleCount, SIGNAL(valueChanged(int)), this,
			 SLOT(ThrottleCountChanged(int)));
	QWidget::connect(_showMatch, SIGNAL(clicked()), this,
			 SLOT(ShowMatchClicked()));

	populateVideoSelection(_videoSelection);
	populateConditionSelection(_condition);

	QHBoxLayout *entryLine1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", _videoSelection},
		{"{{condition}}", _condition},
		{"{{imagePath}}", _imagePath},
		{"{{minNeighbors}}", _minNeighbors},
		{"{{minSizeX}}", _minSizeX},
		{"{{minSizeY}}", _minSizeY},
		{"{{maxSizeX}}", _maxSizeX},
		{"{{maxSizeY}}", _maxSizeY},
		{"{{modelDataPath}}", _modelDataPath},
		{"{{throttleEnable}}", _throttleEnable},
		{"{{throttleCount}}", _throttleCount},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     entryLine1Layout, widgetPlaceholders);

	_modelPathLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.modelPath"),
		_modelPathLayout, widgetPlaceholders);

	_neighborsControlLayout = new QHBoxLayout;
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.minNeighbor"),
		_neighborsControlLayout, widgetPlaceholders);

	_minSizeControlLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.entry.minSize"),
		     _minSizeControlLayout, widgetPlaceholders);

	_maxSizeControlLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.entry.maxSize"),
		     _maxSizeControlLayout, widgetPlaceholders);

	_throttleControlLayout = new QHBoxLayout;
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.entry.throttle"),
		     _throttleControlLayout, widgetPlaceholders);

	QHBoxLayout *showMatchLayout = new QHBoxLayout;
	showMatchLayout->addWidget(_showMatch);
	showMatchLayout->addStretch();
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLine1Layout);
	mainLayout->addWidget(_usePatternForChangedCheck);
	mainLayout->addWidget(_patternThreshold);
	mainLayout->addWidget(_useAlphaAsMask);
	mainLayout->addLayout(_modelPathLayout);
	mainLayout->addWidget(_objectScaleThreshold);
	mainLayout->addLayout(_neighborsControlLayout);
	mainLayout->addWidget(_minNeighborsDescription);
	mainLayout->addLayout(_minSizeControlLayout);
	mainLayout->addLayout(_maxSizeControlLayout);
	mainLayout->addLayout(showMatchLayout);
	mainLayout->addLayout(_throttleControlLayout);
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

void MacroConditionVideoEdit::SourceChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_videoSource = GetWeakSourceByQString(text);
	_entryData->ResetLastMatch();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
}

void MacroConditionVideoEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_condition = static_cast<VideoCondition>(cond);
	_entryData->ResetLastMatch();
	SetWidgetVisibility();

	// Reload image data to avoid incorrect matches.
	//
	// Condition type HAS_NOT_CHANGED will use matchImage to store previous
	// frame of video source, which will differ from the image stored at
	// specified file location.
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}

	if (_entryData->_condition == VideoCondition::OBJECT) {
		auto path = _entryData->GetModelDataPath();
		_entryData->_objectCascade = initObjectCascade(path);
	}
}

void MacroConditionVideoEdit::ImagePathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_file = text.toUtf8().constData();
	_entryData->ResetLastMatch();
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
}

void MacroConditionVideoEdit::ImageBrowseButtonClicked()
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
	_imagePath->SetPath(path);
	ImagePathChanged(path);
}

void MacroConditionVideoEdit::UsePatternForChangedCheckChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_usePatternForChangedCheck = value;
	_patternThreshold->setVisible(value);
	adjustSize();
}

void MacroConditionVideoEdit::PatternThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_patternThreshold = value;
}

void MacroConditionVideoEdit::UseAlphaAsMaskChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_useAlphaAsMask = value;
	_entryData->LoadImageFromFile();
}

void MacroConditionVideoEdit::ObjectScaleThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_scaleFactor = value;
}

void MacroConditionVideoEdit::MinNeighborsChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minNeighbors = value;
}

void MacroConditionVideoEdit::MinSizeXChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minSizeX = value;
}

void MacroConditionVideoEdit::MinSizeYChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minSizeY = value;
}

void MacroConditionVideoEdit::MaxSizeXChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxSizeX = value;
}

void MacroConditionVideoEdit::MaxSizeYChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxSizeY = value;
}

void MacroConditionVideoEdit::ThrottleEnableChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_throttleEnabled = value;
	_throttleCount->setEnabled(value);
}

void MacroConditionVideoEdit::ThrottleCountChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_throttleCount = value / GetSwitcher()->interval;
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
					  cv::Scalar(255, 0, 0, 255), 2, 8, 0);
			}
		}
	}
	return MatToQImage(matchImg);
}

QImage markObjects(QImage &image, std::vector<cv::Rect> &objects)
{
	auto frame = QImageToMat(image);
	for (size_t i = 0; i < objects.size(); i++) {
		rectangle(frame, cv::Point(objects[i].x, objects[i].y),
			  cv::Point(objects[i].x + objects[i].width,
				    objects[i].y + objects[i].height),
			  cv::Scalar(255, 0, 0, 255), 2, 8, 0);
	}
	return MatToQImage(frame);
}

void MacroConditionVideoEdit::ShowMatchClicked()
{
	_matchDialog.show();
	_matchDialog.raise();
	_matchDialog.activateWindow();
	_matchDialog.ShowMatch();
}

void MacroConditionVideoEdit::ModelPathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	bool dataLoaded = false;
	{
		std::lock_guard<std::mutex> lock(GetSwitcher()->m);
		std::string path = text.toStdString();
		dataLoaded = _entryData->LoadModelData(path);
	}
	if (!dataLoaded) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.modelLoadFail"));
	}
}

bool needsShowMatch(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT;
}

bool needsObjectControls(VideoCondition cond)
{
	return cond == VideoCondition::OBJECT;
}

bool needsThrottleControls(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT;
}

bool needsThreshold(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::HAS_CHANGED ||
	       cond == VideoCondition::HAS_NOT_CHANGED;
}

bool patternControlIsOptional(VideoCondition cond)
{
	return cond == VideoCondition::HAS_CHANGED ||
	       cond == VideoCondition::HAS_NOT_CHANGED;
}

void MacroConditionVideoEdit::SetWidgetVisibility()
{
	_imagePath->setVisible(requiresFileInput(_entryData->_condition));
	_usePatternForChangedCheck->setVisible(
		patternControlIsOptional(_entryData->_condition));
	_patternThreshold->setVisible(needsThreshold(_entryData->_condition));
	_useAlphaAsMask->setVisible(_entryData->_condition ==
				    VideoCondition::PATTERN);
	_showMatch->setVisible(needsShowMatch(_entryData->_condition));
	_objectScaleThreshold->setVisible(
		needsObjectControls(_entryData->_condition));
	setLayoutVisible(_neighborsControlLayout,
			 needsObjectControls(_entryData->_condition));
	_minNeighborsDescription->setVisible(
		needsObjectControls(_entryData->_condition));
	setLayoutVisible(_minSizeControlLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_maxSizeControlLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_modelPathLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_throttleControlLayout,
			 needsThrottleControls(_entryData->_condition));

	if (_entryData->_condition == VideoCondition::HAS_CHANGED ||
	    _entryData->_condition == VideoCondition::HAS_NOT_CHANGED) {
		_patternThreshold->setVisible(
			_entryData->_usePatternForChangedCheck);
	}

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
	_imagePath->SetPath(QString::fromStdString(_entryData->_file));
	_usePatternForChangedCheck->setChecked(
		_entryData->_usePatternForChangedCheck);
	_patternThreshold->SetDoubleValue(_entryData->_patternThreshold);
	_useAlphaAsMask->setChecked(_entryData->_useAlphaAsMask);
	_modelDataPath->SetPath(_entryData->GetModelDataPath().c_str());
	_objectScaleThreshold->SetDoubleValue(_entryData->_scaleFactor);
	_minNeighbors->setValue(_entryData->_minNeighbors);
	_minSizeX->setValue(_entryData->_minSizeX);
	_minSizeY->setValue(_entryData->_minSizeY);
	_maxSizeX->setValue(_entryData->_maxSizeX);
	_maxSizeY->setValue(_entryData->_maxSizeY);
	_throttleEnable->setChecked(_entryData->_throttleEnabled);
	_throttleCount->setValue(_entryData->_throttleCount *
				 GetSwitcher()->interval);
	SetWidgetVisibility();
}

ShowMatchDialog::ShowMatchDialog(QWidget *parent,
				 MacroConditionVideo *conditionData)
	: QDialog(parent),
	  _conditionData(conditionData),
	  _imageLabel(new QLabel),
	  _scrollArea(new QScrollArea)
{
	setWindowTitle("Advanced Scene Switcher");
	_statusLabel = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.showMatch.loading"));

	_scrollArea->setBackgroundRole(QPalette::Dark);
	_scrollArea->setWidget(_imageLabel);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_statusLabel);
	layout->addWidget(_scrollArea);
	setLayout(layout);
}

ShowMatchDialog::~ShowMatchDialog()
{
	_stop = true;
	if (_thread.joinable()) {
		_thread.join();
	}
}

void ShowMatchDialog::ShowMatch()
{
	if (_thread.joinable()) {
		return;
	}
	if (!_conditionData) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		return;
	}
	_thread = std::thread(&ShowMatchDialog::CheckForMatchLoop, this);
}

void ShowMatchDialog::RedrawImage(QImage img)
{
	_imageLabel->setPixmap(QPixmap::fromImage(img));
	_imageLabel->adjustSize();
}

void ShowMatchDialog::CheckForMatchLoop()
{
	while (!_stop) {
		auto source = obs_weak_source_get_source(
			_conditionData->_videoSource);
		AdvSSScreenshotObj screenshot(source);
		obs_source_release(source);
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (!screenshot.done) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotFail"));
			continue;
		}
		if (screenshot.image.width() == 0 ||
		    screenshot.image.height() == 0) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotEmpty"));
			continue;
		}
		auto image = MarkMatch(screenshot.image);
		if (_stop) {
			return;
		}
		QMetaObject::invokeMethod(this, "RedrawImage",
					  Qt::BlockingQueuedConnection,
					  Q_ARG(QImage, image));
	}
}

QImage ShowMatchDialog::MarkMatch(QImage &screenshot)
{
	QImage resultIamge;
	if (_conditionData->_condition == VideoCondition::PATTERN) {
		cv::Mat result;
		QImage pattern = _conditionData->GetMatchImage();
		matchPattern(screenshot, pattern,
			     _conditionData->_patternThreshold, result,
			     _conditionData->_useAlphaAsMask);
		if (countNonZero(result) == 0) {
			resultIamge = screenshot;
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchSuccess"));
			resultIamge = markPatterns(result, screenshot, pattern);
		}
	} else if (_conditionData->_condition == VideoCondition::OBJECT) {
		auto objects = matchObject(
			screenshot, _conditionData->_objectCascade,
			_conditionData->_scaleFactor,
			_conditionData->_minNeighbors,
			{_conditionData->_minSizeX, _conditionData->_minSizeY},
			{_conditionData->_maxSizeX, _conditionData->_maxSizeY});
		if (objects.empty()) {
			resultIamge = screenshot;
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchSuccess"));
			resultIamge = markObjects(screenshot, objects);
		}
	}
	return resultIamge;
}
