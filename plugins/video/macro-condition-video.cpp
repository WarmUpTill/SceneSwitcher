#include "macro-condition-video.hpp"

#include <layout-helpers.hpp>
#include <macro-condition-edit.hpp>
#include <plugin-state-helpers.hpp>
#include <QBuffer>
#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QtGlobal>
#include <QToolTip>
#include <ui-helpers.hpp>
#include <selection-helpers.hpp>

namespace advss {

const std::string MacroConditionVideo::id = "video";

bool MacroConditionVideo::_registered = MacroConditionFactory::Register(
	MacroConditionVideo::id,
	{MacroConditionVideo::Create, MacroConditionVideoEdit::Create,
	 "AdvSceneSwitcher.condition.video"});

const static std::map<VideoCondition, std::string> conditionTypes = {
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
	{VideoCondition::BRIGHTNESS,
	 "AdvSceneSwitcher.condition.video.condition.brightness"},
#ifdef OCR_SUPPORT
	{VideoCondition::OCR, "AdvSceneSwitcher.condition.video.condition.ocr"},
#endif
	{VideoCondition::COLOR,
	 "AdvSceneSwitcher.condition.video.condition.color"},
};

const static std::map<VideoInput::Type, std::string> videoInputTypes = {
	{VideoInput::Type::OBS_MAIN_OUTPUT,
	 "AdvSceneSwitcher.condition.video.type.main"},
	{VideoInput::Type::SOURCE,
	 "AdvSceneSwitcher.condition.video.type.source"},
	{VideoInput::Type::SCENE,
	 "AdvSceneSwitcher.condition.video.type.scene"},
};

const static std::map<cv::TemplateMatchModes, std::string> patternMatchModes = {
	{cv::TemplateMatchModes::TM_CCOEFF_NORMED,
	 "AdvSceneSwitcher.condition.video.patternMatchMode.correlationCoefficient"},
	{cv::TemplateMatchModes::TM_CCORR_NORMED,
	 "AdvSceneSwitcher.condition.video.patternMatchMode.crossCorrelation"},
	{cv::TemplateMatchModes::TM_SQDIFF_NORMED,
	 "AdvSceneSwitcher.condition.video.patternMatchMode.squaredDifference"},
};

const static std::map<tesseract::PageSegMode, std::string> pageSegModes = {
	{tesseract::PageSegMode::PSM_SINGLE_COLUMN,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleColumn"},
	{tesseract::PageSegMode::PSM_SINGLE_BLOCK_VERT_TEXT,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleBlockVertText"},
	{tesseract::PageSegMode::PSM_SINGLE_BLOCK,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleBlock"},
	{tesseract::PageSegMode::PSM_SINGLE_LINE,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleLine"},
	{tesseract::PageSegMode::PSM_SINGLE_WORD,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleWord"},
	{tesseract::PageSegMode::PSM_CIRCLE_WORD,
	 "AdvSceneSwitcher.condition.video.ocrMode.circleWord"},
	{tesseract::PageSegMode::PSM_SINGLE_CHAR,
	 "AdvSceneSwitcher.condition.video.ocrMode.singleChar"},
	{tesseract::PageSegMode::PSM_SPARSE_TEXT,
	 "AdvSceneSwitcher.condition.video.ocrMode.sparseText"},
	{tesseract::PageSegMode::PSM_SPARSE_TEXT_OSD,
	 "AdvSceneSwitcher.condition.video.ocrMode.sparseTextOSD"},
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

static bool requiresFileInput(VideoCondition t)
{
	return t == VideoCondition::MATCH || t == VideoCondition::DIFFER ||
	       t == VideoCondition::PATTERN;
}

bool MacroConditionVideo::CheckShouldBeSkipped()
{
	if (_condition != VideoCondition::PATTERN &&
	    _condition != VideoCondition::OBJECT &&
	    _condition != VideoCondition::HAS_CHANGED &&
	    _condition != VideoCondition::HAS_NOT_CHANGED) {
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
	if (!_video.ValidSelection()) {
		return false;
	}

	bool match = false;
	if (CheckShouldBeSkipped()) {
		return _lastMatchResult;
	}

	if (_blockUntilScreenshotDone) {
		GetScreenshot(true);
	}

	if (_screenshotData.done) {
		match = Compare();
		_lastMatchResult = match;

		if (!requiresFileInput(_condition)) {
			_matchImage = std::move(_screenshotData.image);
		}
		_getNextScreenshot = true;
	} else {
		match = _lastMatchResult;
	}

	if (!_blockUntilScreenshotDone && _getNextScreenshot) {
		GetScreenshot();
	}
	return match;
}

bool MacroConditionVideo::Save(obs_data_t *obj) const
{
	MacroCondition::Save(obj);
	_video.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "filePath", _file.c_str());
	obs_data_set_bool(obj, "blockUntilScreenshotDone",
			  _blockUntilScreenshotDone);
	_brightnessThreshold.Save(obj, "brightnessThreshold");
	_patternMatchParameters.Save(obj);
	_objMatchParameters.Save(obj);
	_ocrParameters.Save(obj);
	_colorParameters.Save(obj);
	obs_data_set_bool(obj, "throttleEnabled", _throttleEnabled);
	obs_data_set_int(obj, "throttleCount", _throttleCount);
	_areaParameters.Save(obj);
	return true;
}

bool MacroConditionVideo::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_video.Load(obj);
	SetCondition(static_cast<VideoCondition>(
		obs_data_get_int(obj, "condition")));
	_file = obs_data_get_string(obj, "filePath");
	_blockUntilScreenshotDone =
		obs_data_get_bool(obj, "blockUntilScreenshotDone");
	// TODO: Remove this fallback in a future version
	if (obs_data_has_user_value(obj, "brightness")) {
		_brightnessThreshold = obs_data_get_double(obj, "brightness");
	} else {
		_brightnessThreshold.Load(obj, "brightnessThreshold");
	}
	_patternMatchParameters.Load(obj);
	_objMatchParameters.Load(obj);
	_ocrParameters.Load(obj);
	_colorParameters.Load(obj);
	_throttleEnabled = obs_data_get_bool(obj, "throttleEnabled");
	_throttleCount = obs_data_get_int(obj, "throttleCount");
	_areaParameters.Load(obj);
	if (requiresFileInput(_condition)) {
		(void)LoadImageFromFile();
	}

	if (_condition == VideoCondition::OBJECT) {
		LoadModelData(_objMatchParameters.modelPath);
	}

	return true;
}

std::string MacroConditionVideo::GetShortDesc() const
{
	return _video.ToString();
}

void MacroConditionVideo::GetScreenshot(bool blocking)
{
	auto source = obs_weak_source_get_source(_video.GetVideo());
	_screenshotData.~ScreenshotHelper();
	QRect screenshotArea;
	if (_areaParameters.enable && _condition != VideoCondition::NO_IMAGE) {
		screenshotArea.setRect(_areaParameters.area.x,
				       _areaParameters.area.y,
				       _areaParameters.area.width,
				       _areaParameters.area.height);
	}
	new (&_screenshotData) ScreenshotHelper(source, screenshotArea,
						blocking, GetIntervalValue());
	obs_source_release(source);
	_getNextScreenshot = false;
}

bool MacroConditionVideo::LoadImageFromFile()
{
	if (!_matchImage.load(QString::fromStdString(_file))) {
		blog(LOG_WARNING, "Cannot load image data from file '%s'",
		     _file.c_str());
		(&_matchImage)->~QImage();
		new (&_matchImage) QImage();
		_patternImageData = {};
		return false;
	}

	_matchImage =
		_matchImage.convertToFormat(QImage::Format::Format_RGBA8888);
	_patternMatchParameters.image = _matchImage;
	_patternImageData = CreatePatternData(_matchImage);
	return true;
}

bool MacroConditionVideo::LoadModelData(std::string &path)
{
	_objMatchParameters.modelPath = path;
	_objMatchParameters.cascade = initObjectCascade(path);
	return !_objMatchParameters.cascade.empty();
}

std::string MacroConditionVideo::GetModelDataPath() const
{
	return _objMatchParameters.modelPath;
}

void MacroConditionVideo::SetPageSegMode(tesseract::PageSegMode mode)
{
	_ocrParameters.SetPageMode(mode);
}

bool MacroConditionVideo::SetLanguage(const std::string &language)
{
	return _ocrParameters.SetLanguageCode(language);
}

void MacroConditionVideo::SetCondition(VideoCondition condition)
{
	_condition = condition;
	SetupTempVars();
}

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	MatchPattern(_screenshotData.image, _patternImageData,
		     _patternMatchParameters.threshold, result, nullptr,
		     _patternMatchParameters.useAlphaAsMask,
		     _patternMatchParameters.matchMode);
	if (result.total() == 0) {
		SetTempVarValue("patternCount", "0");
		return false;
	}
	const auto count = countNonZero(result);
	SetTempVarValue("patternCount", std::to_string(count));
	return count > 0;
}

bool MacroConditionVideo::OutputChanged()
{
	if (!_patternMatchParameters.useForChangedCheck) {
		return _screenshotData.image != _matchImage;
	}

	cv::Mat result;
	_patternImageData = CreatePatternData(_matchImage);
	MatchPattern(_screenshotData.image, _patternImageData,
		     _patternMatchParameters.threshold, result, nullptr,
		     _patternMatchParameters.useAlphaAsMask,
		     _patternMatchParameters.matchMode);
	if (result.total() == 0) {
		return false;
	}
	return countNonZero(result) == 0;
}

bool MacroConditionVideo::ScreenshotContainsObject()
{
	auto objects = MatchObject(_screenshotData.image,
				   _objMatchParameters.cascade,
				   _objMatchParameters.scaleFactor,
				   _objMatchParameters.minNeighbors,
				   _objMatchParameters.minSize.CV(),
				   _objMatchParameters.maxSize.CV());
	const auto count = objects.size();
	SetTempVarValue("objectCount", std::to_string(count));
	return count > 0;
}

bool MacroConditionVideo::CheckBrightnessThreshold()
{
	_currentBrightness = GetAvgBrightness(_screenshotData.image) / 255.;
	SetTempVarValue("brightness", std::to_string(_currentBrightness));
	return _currentBrightness > _brightnessThreshold;
}

bool MacroConditionVideo::CheckOCR()
{
	if (!_ocrParameters.Initialized()) {
		return false;
	}

	auto text = RunOCR(_ocrParameters.GetOCR(), _screenshotData.image,
			   _ocrParameters.color, _ocrParameters.colorThreshold);
	SetVariableValue(text);
	SetTempVarValue("text", text);
	if (!_ocrParameters.regex.Enabled()) {
		return text == std::string(_ocrParameters.text);
	}
	return _ocrParameters.regex.Matches(text, _ocrParameters.text);
}

bool MacroConditionVideo::CheckColor()
{
	const bool ret = ContainsPixelsInColorRange(
		_screenshotData.image, _colorParameters.color,
		_colorParameters.colorThreshold,
		_colorParameters.matchThreshold);
	// Way too slow for now
	//SetTempVarValue("dominantColor", GetDominantColor(_screenshotData.image, 3)
	//				 .name(QColor::HexArgb)
	//				 .toStdString());
	SetTempVarValue("color", GetAverageColor(_screenshotData.image)
					 .name(QColor::HexArgb)
					 .toStdString());
	return ret;
}

bool MacroConditionVideo::Compare()
{
	if (_condition != VideoCondition::OCR) {
		SetVariableValue("");
	}

	switch (_condition) {
	case VideoCondition::MATCH:
		return _screenshotData.image == _matchImage;
	case VideoCondition::DIFFER:
		return _screenshotData.image != _matchImage;
	case VideoCondition::HAS_CHANGED:
		return OutputChanged();
	case VideoCondition::HAS_NOT_CHANGED:
		return !OutputChanged();
	case VideoCondition::NO_IMAGE:
		return _screenshotData.image.isNull();
	case VideoCondition::PATTERN:
		return ScreenshotContainsPattern();
	case VideoCondition::OBJECT:
		return ScreenshotContainsObject();
	case VideoCondition::BRIGHTNESS:
		return CheckBrightnessThreshold();
	case VideoCondition::OCR:
		return CheckOCR();
	case VideoCondition::COLOR:
		return CheckColor();
	default:
		break;
	}
	return false;
}

void MacroConditionVideo::SetupTempVars()
{
	MacroCondition::SetupTempVars();
	switch (_condition) {
	case VideoCondition::PATTERN:
		AddTempvar(
			"patternCount",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.patternCount"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.patternCount.description"));
		break;
	case VideoCondition::OBJECT:
		AddTempvar(
			"objectCount",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.objectCount"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.objectCount.description"));
		break;
	case VideoCondition::BRIGHTNESS:
		AddTempvar(
			"brightness",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.brightness"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.brightness.description"));
		break;
	case VideoCondition::OCR:
		AddTempvar(
			"text",
			obs_module_text("AdvSceneSwitcher.tempVar.video.text"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.text.description"));
		break;
	case VideoCondition::COLOR:
		AddTempvar(
			"color",
			obs_module_text("AdvSceneSwitcher.tempVar.video.color"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.color.description"));
		break;
	case VideoCondition::MATCH:
	case VideoCondition::DIFFER:
	case VideoCondition::HAS_NOT_CHANGED:
	case VideoCondition::HAS_CHANGED:
	case VideoCondition::NO_IMAGE:
	default:
		break;
	}
}

static inline void populateVideoInputSelection(QComboBox *list)
{
	for (const auto &[_, name] : videoInputTypes) {
		list->addItem(obs_module_text(name.c_str()));
	}
}

static inline void populateConditionSelection(QComboBox *list)
{
	for (auto entry : conditionTypes) {
		list->addItem(obs_module_text(entry.second.c_str()));
	}
}

static inline void populatePageSegModeSelection(QComboBox *list)
{
	for (const auto &[mode, name] : pageSegModes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(mode));
	}
}

static inline void populatePatternMatchModeSelection(QComboBox *list)
{
	for (const auto &[mode, name] : patternMatchModes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(mode));
	}
}

BrightnessEdit::BrightnessEdit(QWidget *parent,
			       const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _threshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThresholdDescription"))),
	  _current(new QLabel),
	  _data(data)
{
	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(_threshold);
	layout->addWidget(_current);
	setLayout(layout);

	QWidget::connect(
		_threshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(BrightnessThresholdChanged(
			const NumberVariable<double> &)));
	QWidget::connect(&_timer, &QTimer::timeout, this,
			 &BrightnessEdit::UpdateCurrentBrightness);
	_timer.start(1000);

	_threshold->SetDoubleValue(_data->_brightnessThreshold);
	_loading = false;
}

void BrightnessEdit::UpdateCurrentBrightness()
{
	QString text = obs_module_text(
		"AdvSceneSwitcher.condition.video.currentBrightness");
	_current->setText(text.arg(_data->GetCurrentBrightness()));
}

void BrightnessEdit::BrightnessThresholdChanged(const DoubleVariable &value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_brightnessThreshold = value;
}

OCREdit::OCREdit(QWidget *parent, PreviewDialog *previewDialog,
		 const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _textColor(new QLabel),
	  _selectColor(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectColor"))),
	  _colorThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThresholdDescription"),
		  true)),
	  _pageSegMode(new QComboBox()),
	  _languageCode(new VariableLineEdit(this)),
	  _previewDialog(previewDialog),
	  _data(data)
{
	populatePageSegModeSelection(_pageSegMode);

	QWidget::connect(_selectColor, SIGNAL(clicked()), this,
			 SLOT(SelectColorClicked()));
	QWidget::connect(
		_colorThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ColorThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex,
			 SIGNAL(RegexConfigChanged(const RegexConfig &)), this,
			 SLOT(RegexChanged(const RegexConfig &)));
	QWidget::connect(_pageSegMode, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PageSegModeChanged(int)));
	QWidget::connect(_languageCode, SIGNAL(editingFinished()), this,
			 SLOT(LanguageChanged()));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{textColor}}", _textColor},
		{"{{selectColor}}", _selectColor},
		{"{{textType}}", _pageSegMode},
		{"{{languageCode}}", _languageCode},
	};

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	auto textLayout = new QHBoxLayout();
	textLayout->setContentsMargins(0, 0, 0, 0);
	textLayout->addWidget(_matchText);
	textLayout->addWidget(_regex);
	layout->addLayout(textLayout);
	auto pageModeSegLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.orcTextType"),
		pageModeSegLayout, widgetPlaceholders);
	layout->addLayout(pageModeSegLayout);
	auto languageLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.orcLanguage"),
		languageLayout, widgetPlaceholders);
	layout->addLayout(languageLayout);
	auto colorPickLayout = new QHBoxLayout();
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.orcColorPick"),
		colorPickLayout, widgetPlaceholders);
	layout->addLayout(colorPickLayout);
	layout->addWidget(_colorThreshold);
	setLayout(layout);

	_matchText->setPlainText(_data->_ocrParameters.text);
	_regex->SetRegexConfig(_data->_ocrParameters.regex);
	SetupColorLabel(_data->_ocrParameters.color);
	_colorThreshold->SetDoubleValue(_data->_ocrParameters.colorThreshold);
	_pageSegMode->setCurrentIndex(_pageSegMode->findData(
		static_cast<int>(_data->_ocrParameters.GetPageMode())));
	_languageCode->setText(_data->_ocrParameters.GetLanguageCode());
	_loading = false;
}

void OCREdit::SetupColorLabel(const QColor &color)
{
	_textColor->setText(color.name());
	_textColor->setPalette(QPalette(color));
	_textColor->setAutoFillBackground(true);
}

void OCREdit::SelectColorClicked()
{
	if (_loading || !_data) {
		return;
	}

	const QColor color = QColorDialog::getColor(
		_data->_ocrParameters.color, this,
		obs_module_text("AdvSceneSwitcher.condition.video.selectColor"),
		QColorDialog::ColorDialogOption());

	if (!color.isValid()) {
		return;
	}

	SetupColorLabel(color);
	auto lock = LockContext();
	_data->_ocrParameters.color = color;

	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

void OCREdit::ColorThresholdChanged(const DoubleVariable &value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_ocrParameters.colorThreshold = value;

	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

void OCREdit::MatchTextChanged()
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_ocrParameters.text =
		_matchText->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();

	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

void OCREdit::RegexChanged(const RegexConfig &conf)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_ocrParameters.regex = conf;
	adjustSize();
	updateGeometry();

	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

void OCREdit::PageSegModeChanged(int idx)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->SetPageSegMode(static_cast<tesseract::PageSegMode>(
		_pageSegMode->itemData(idx).toInt()));

	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

void OCREdit::LanguageChanged()
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	if (!_data->SetLanguage(_languageCode->text().toStdString())) {
		const QString message(obs_module_text(
			"AdvSceneSwitcher.condition.video.ocrLanguageNotFound"));
		const QDir dataDir(
			obs_get_module_data_path(obs_current_module()));
		const QString fileName(_languageCode->text() + ".traineddata");
		DisplayMessage(message.arg(fileName, dataDir.absolutePath()));

		// Reset to previous value
		const QSignalBlocker b(this);
		_languageCode->setText(_data->_ocrParameters.GetLanguageCode());
		return;
	}
	_previewDialog->OCRParametersChanged(_data->_ocrParameters);
}

ObjectDetectEdit::ObjectDetectEdit(
	QWidget *parent, PreviewDialog *previewDialog,
	const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _modelDataPath(new FileSelection()),
	  _objectScaleThreshold(new SliderSpinBox(
		  1.1, 5.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThresholdDescription"))),
	  _minNeighbors(new QSpinBox()),
	  _minNeighborsDescription(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.video.minNeighborDescription"))),
	  _minSize(new SizeSelection(0, 1024)),
	  _maxSize(new SizeSelection(0, 4096)),
	  _previewDialog(previewDialog),
	  _data(data)
{
	_minNeighbors->setMinimum(minMinNeighbors);
	_minNeighbors->setMaximum(maxMinNeighbors);

	QWidget::connect(
		_objectScaleThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ObjectScaleThresholdChanged(
			const NumberVariable<double> &)));
	QWidget::connect(_minNeighbors, SIGNAL(valueChanged(int)), this,
			 SLOT(MinNeighborsChanged(int)));
	QWidget::connect(_minSize, SIGNAL(SizeChanged(Size)), this,
			 SLOT(MinSizeChanged(Size)));
	QWidget::connect(_maxSize, SIGNAL(SizeChanged(Size)), this,
			 SLOT(MaxSizeChanged(Size)));
	QWidget::connect(_modelDataPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ModelPathChanged(const QString &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{minNeighbors}}", _minNeighbors},
		{"{{minSize}}", _minSize},
		{"{{maxSize}}", _maxSize},
		{"{{modelDataPath}}", _modelDataPath},
	};

	auto pathLayout = new QHBoxLayout;
	pathLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.modelPath"),
		pathLayout, widgetPlaceholders);

	auto neighborsLayout = new QHBoxLayout;
	neighborsLayout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.minNeighbor"),
		neighborsLayout, widgetPlaceholders);

	auto sizeGrid = new QGridLayout;
	sizeGrid->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.video.minSize")),
		0, 0);
	sizeGrid->addWidget(_minSize, 0, 1);
	sizeGrid->addWidget(
		new QLabel(obs_module_text(
			"AdvSceneSwitcher.condition.video.maxSize")),
		1, 0);
	sizeGrid->addWidget(_maxSize, 1, 1);
	auto sizeLayout = new QHBoxLayout;
	sizeLayout->setContentsMargins(0, 0, 0, 0);
	sizeLayout->addLayout(sizeGrid);
	sizeLayout->addStretch();

	auto layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(pathLayout);
	layout->addLayout(neighborsLayout);
	layout->addLayout(sizeLayout);
	setLayout(layout);

	_modelDataPath->SetPath(_data->GetModelDataPath());
	_objectScaleThreshold->SetDoubleValue(
		_data->_objMatchParameters.scaleFactor);
	_minNeighbors->setValue(_data->_objMatchParameters.minNeighbors);
	_minSize->SetSize(_data->_objMatchParameters.minSize);
	_maxSize->SetSize(_data->_objMatchParameters.maxSize);
	_loading = false;
}

void ObjectDetectEdit::ObjectScaleThresholdChanged(const DoubleVariable &value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_objMatchParameters.scaleFactor = value;
	_previewDialog->ObjDetectParametersChanged(_data->_objMatchParameters);
}

void ObjectDetectEdit::MinNeighborsChanged(int value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_objMatchParameters.minNeighbors = value;
	_previewDialog->ObjDetectParametersChanged(_data->_objMatchParameters);
}

void ObjectDetectEdit::MinSizeChanged(advss::Size value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_objMatchParameters.minSize = value;
	_previewDialog->ObjDetectParametersChanged(_data->_objMatchParameters);
}

void ObjectDetectEdit::MaxSizeChanged(advss::Size value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_objMatchParameters.maxSize = value;
	_previewDialog->ObjDetectParametersChanged(_data->_objMatchParameters);
}

void ObjectDetectEdit::ModelPathChanged(const QString &text)
{
	if (_loading || !_data) {
		return;
	}

	bool dataLoaded = false;
	{
		auto lock = LockContext();
		std::string path = text.toStdString();
		dataLoaded = _data->LoadModelData(path);
	}
	if (!dataLoaded) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.modelLoadFail"));
	}
	_previewDialog->ObjDetectParametersChanged(_data->_objMatchParameters);
}

ColorEdit::ColorEdit(QWidget *parent,
		     const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _matchThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorMatchThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorMatchThresholdDescription"),
		  true)),
	  _colorThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.colorDeviationThresholdDescription"),
		  true)),
	  _color(new QLabel),
	  _selectColor(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectColor"))),
	  _data(data)
{
	QWidget::connect(_selectColor, SIGNAL(clicked()), this,
			 SLOT(SelectColorClicked()));
	QWidget::connect(
		_matchThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(MatchThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(
		_colorThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(ColorThresholdChanged(const NumberVariable<double> &)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{color}}", _color},
		{"{{selectColor}}", _selectColor},
	};

	auto colorLayout = new QHBoxLayout;
	PlaceWidgets(
		obs_module_text("AdvSceneSwitcher.condition.video.entry.color"),
		colorLayout, widgetPlaceholders);

	auto layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(colorLayout);
	layout->addWidget(_colorThreshold);
	layout->addWidget(_matchThreshold);
	setLayout(layout);

	_matchThreshold->SetDoubleValue(_data->_colorParameters.matchThreshold);
	_colorThreshold->SetDoubleValue(_data->_colorParameters.colorThreshold);
	SetupColorLabel(_data->_colorParameters.color);
	_loading = false;
}

void ColorEdit::SetupColorLabel(const QColor &color)
{
	_color->setText(color.name());
	_color->setPalette(QPalette(color));
	_color->setAutoFillBackground(true);
}

void ColorEdit::MatchThresholdChanged(const DoubleVariable &value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_colorParameters.matchThreshold = value;
}

void ColorEdit::ColorThresholdChanged(const DoubleVariable &value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_colorParameters.colorThreshold = value;
}

void ColorEdit::SelectColorClicked()
{
	if (_loading || !_data) {
		return;
	}

	const QColor color = QColorDialog::getColor(
		_data->_colorParameters.color, this,
		obs_module_text("AdvSceneSwitcher.condition.video.selectColor"),
		QColorDialog::ColorDialogOption());

	if (!color.isValid()) {
		return;
	}

	SetupColorLabel(color);
	auto lock = LockContext();
	_data->_colorParameters.color = color;
}

AreaEdit::AreaEdit(QWidget *parent, PreviewDialog *previewDialog,
		   const std::shared_ptr<MacroConditionVideo> &data)
	: QWidget(parent),
	  _checkAreaEnable(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.entry.checkAreaEnable"))),
	  _checkArea(new AreaSelection(0, 99999)),
	  _selectArea(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectArea"))),
	  _previewDialog(previewDialog),
	  _data(data)
{
	QWidget::connect(_checkAreaEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckAreaEnableChanged(int)));
	QWidget::connect(_checkArea, SIGNAL(AreaChanged(Area)), this,
			 SLOT(CheckAreaChanged(Area)));
	QWidget::connect(_selectArea, SIGNAL(clicked()), this,
			 SLOT(SelectAreaClicked()));
	QWidget::connect(_previewDialog, SIGNAL(SelectionAreaChanged(QRect)),
			 this, SLOT(CheckAreaChanged(QRect)));

	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{checkAreaEnable}}", _checkAreaEnable},
		{"{{checkArea}}", _checkArea},
		{"{{selectArea}}", _selectArea},
	};

	auto layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.checkArea"),
		layout, widgetPlaceholders);
	setLayout(layout);

	_checkAreaEnable->setChecked(_data->_areaParameters.enable);
	_checkArea->SetArea(_data->_areaParameters.area);
	SetWidgetVisibility();
	_loading = false;
}

void AreaEdit::SetWidgetVisibility()
{
	_checkArea->setVisible(_data->_areaParameters.enable);
	_selectArea->setVisible(_data->_areaParameters.enable);
	adjustSize();
	updateGeometry();
}

void AreaEdit::SelectAreaClicked()
{
	_previewDialog->show();
	_previewDialog->raise();
	_previewDialog->activateWindow();
	_previewDialog->SelectArea();
}

void AreaEdit::CheckAreaEnableChanged(int value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_areaParameters.enable = value;
	SetWidgetVisibility();
	_previewDialog->AreaParametersChanged(_data->_areaParameters);
	emit Resized();
}

void AreaEdit::CheckAreaChanged(Area value)
{
	if (_loading || !_data) {
		return;
	}

	auto lock = LockContext();
	_data->_areaParameters.area = value;
	_previewDialog->AreaParametersChanged(_data->_areaParameters);
}

void AreaEdit::CheckAreaChanged(QRect rect)
{
	const QSignalBlocker b(_checkArea);
	Area area{rect.topLeft().x(), rect.y(), rect.width(), rect.height()};
	_checkArea->SetArea(area);
	CheckAreaChanged(area);
}

MacroConditionVideoEdit::MacroConditionVideoEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVideo> entryData)
	: QWidget(parent),
	  _videoInputTypes(new QComboBox()),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sources(new SourceSelectionWidget(this, QStringList(), true)),
	  _condition(new QComboBox()),
	  _reduceLatency(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.reduceLatency"))),
	  _usePatternForChangedCheck(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.usePatternForChangedCheck"))),
	  _imagePath(new FileSelection()),
	  _patternThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.patternThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.patternThresholdDescription"))),
	  _useAlphaAsMask(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.patternThresholdUseAlphaAsMask"))),
	  _patternMatchModeLayout(new QHBoxLayout()),
	  _patternMatchMode(new QComboBox()),
	  _showMatch(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.showMatch"))),
	  _previewDialog(this),
	  _brightness(new BrightnessEdit(this, entryData)),
	  _ocr(new OCREdit(this, &_previewDialog, entryData)),
	  _objectDetect(new ObjectDetectEdit(this, &_previewDialog, entryData)),
	  _color(new ColorEdit(this, entryData)),
	  _area(new AreaEdit(this, &_previewDialog, entryData)),
	  _throttleControlLayout(new QHBoxLayout),
	  _throttleEnable(new QCheckBox()),
	  _throttleCount(new QSpinBox())
{
	_reduceLatency->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.reduceLatency.tooltip"));
	_imagePath->Button()->disconnect();
	_usePatternForChangedCheck->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.usePatternForChangedCheck.tooltip"));
	_patternMatchMode->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.patternMatchMode.tip"));
	populatePatternMatchModeSelection(_patternMatchMode);

	_throttleCount->setMinimum(1 * GetIntervalValue());
	_throttleCount->setMaximum(10 * GetIntervalValue());
	_throttleCount->setSingleStep(GetIntervalValue());

	_brightness->setSizePolicy(QSizePolicy::MinimumExpanding,
				   QSizePolicy::Preferred);
	_ocr->setSizePolicy(QSizePolicy::MinimumExpanding,
			    QSizePolicy::Preferred);
	_objectDetect->setSizePolicy(QSizePolicy::MinimumExpanding,
				     QSizePolicy::Preferred);
	_color->setSizePolicy(QSizePolicy::MinimumExpanding,
			      QSizePolicy::Preferred);
	_area->setSizePolicy(QSizePolicy::MinimumExpanding,
			     QSizePolicy::Preferred);

	auto sources = GetVideoSourceNames();
	sources.sort();
	_sources->SetSourceNameList(sources);

	QWidget::connect(_videoInputTypes, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(VideoInputTypeChanged(int)));
	QWidget::connect(_sources,
			 SIGNAL(SourceChanged(const SourceSelection &)), this,
			 SLOT(SourceChanged(const SourceSelection &)));
	QWidget::connect(_scenes, SIGNAL(SceneChanged(const SceneSelection &)),
			 this, SLOT(SceneChanged(const SceneSelection &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(ConditionChanged(int)));
	QWidget::connect(_reduceLatency, SIGNAL(stateChanged(int)), this,
			 SLOT(ReduceLatencyChanged(int)));
	QWidget::connect(_imagePath, SIGNAL(PathChanged(const QString &)), this,
			 SLOT(ImagePathChanged(const QString &)));
	QWidget::connect(_imagePath->Button(), SIGNAL(clicked()), this,
			 SLOT(ImageBrowseButtonClicked()));
	QWidget::connect(_usePatternForChangedCheck, SIGNAL(stateChanged(int)),
			 this, SLOT(UsePatternForChangedCheckChanged(int)));
	QWidget::connect(
		_patternThreshold,
		SIGNAL(DoubleValueChanged(const NumberVariable<double> &)),
		this,
		SLOT(PatternThresholdChanged(const NumberVariable<double> &)));
	QWidget::connect(_useAlphaAsMask, SIGNAL(stateChanged(int)), this,
			 SLOT(UseAlphaAsMaskChanged(int)));
	QWidget::connect(_patternMatchMode, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(PatternMatchModeChanged(int)));

	QWidget::connect(_throttleEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(ThrottleEnableChanged(int)));
	QWidget::connect(_throttleCount, SIGNAL(valueChanged(int)), this,
			 SLOT(ThrottleCountChanged(int)));
	QWidget::connect(_showMatch, SIGNAL(clicked()), this,
			 SLOT(ShowMatchClicked()));
	QWidget::connect(this,
			 SIGNAL(VideoSelectionChanged(const VideoInput &)),
			 &_previewDialog,
			 SLOT(VideoSelectionChanged(const VideoInput &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)),
			 &_previewDialog, SLOT(ConditionChanged(int)));
	QWidget::connect(_area, SIGNAL(Resized()), this, SLOT(Resize()));

	populateVideoInputSelection(_videoInputTypes);
	populateConditionSelection(_condition);

	_patternMatchModeLayout->setContentsMargins(0, 0, 0, 0);
	_throttleControlLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout *entryLine1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoInputTypes}}", _videoInputTypes},
		{"{{sources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{condition}}", _condition},
		{"{{reduceLatency}}", _reduceLatency},
		{"{{imagePath}}", _imagePath},
		{"{{throttleEnable}}", _throttleEnable},
		{"{{throttleCount}}", _throttleCount},
		{"{{patternMatchingModes}}", _patternMatchMode},
	};
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     entryLine1Layout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.patternMatchMode"),
		_patternMatchModeLayout, widgetPlaceholders);
	PlaceWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.entry.throttle"),
		     _throttleControlLayout, widgetPlaceholders);

	QHBoxLayout *showMatchLayout = new QHBoxLayout;
	showMatchLayout->addWidget(_showMatch);
	showMatchLayout->addStretch();
	auto mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLine1Layout);
	mainLayout->addWidget(_usePatternForChangedCheck);
	mainLayout->addWidget(_patternThreshold);
	mainLayout->addWidget(_useAlphaAsMask);
	mainLayout->addLayout(_patternMatchModeLayout);
	mainLayout->addWidget(_brightness);
	mainLayout->addWidget(_ocr);
	mainLayout->addWidget(_objectDetect);
	mainLayout->addWidget(_color);
	mainLayout->addLayout(_throttleControlLayout);
	mainLayout->addWidget(_area);
	mainLayout->addWidget(_reduceLatency);
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

	if (!requiresFileInput(_entryData->GetCondition())) {
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

void MacroConditionVideoEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_video.source = source;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::SceneChanged(const SceneSelection &scene)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_video.scene = scene;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::VideoInputTypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_video.type = static_cast<VideoInput::Type>(type);
	HandleVideoInputUpdate();
	SetWidgetVisibility();
}

void MacroConditionVideoEdit::HandleVideoInputUpdate()
{
	_entryData->ResetLastMatch();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	emit VideoSelectionChanged(_entryData->_video);
}

void MacroConditionVideoEdit::ConditionChanged(int cond)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->SetCondition(static_cast<VideoCondition>(cond));
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
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);

	if (_entryData->GetCondition() == VideoCondition::OBJECT) {
		auto path = _entryData->GetModelDataPath();
		_entryData->_objMatchParameters.cascade =
			initObjectCascade(path);
	}

	SetupPreviewDialogParams();
}

void MacroConditionVideoEdit::ImagePathChanged(const QString &text)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_file = text.toUtf8().constData();
	_entryData->ResetLastMatch();
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ImageBrowseButtonClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	QString path;
	bool useExistingFile = false;
	// Ask whether to create screenshot or to select existing file
	if (_entryData->_video.ValidSelection()) {
		QMessageBox msgBox(
			QMessageBox::Question,
			obs_module_text("AdvSceneSwitcher.windowTitle"),
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction"),
			QMessageBox::Yes | QMessageBox::No |
				QMessageBox::Cancel);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		msgBox.setButtonText(
			QMessageBox::Yes,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.file"));
		msgBox.setButtonText(
			QMessageBox::No,
			obs_module_text(
				"AdvSceneSwitcher.condition.video.askFileAction.screenshot"));
#else
		auto yes = msgBox.button(QMessageBox::StandardButton::Yes);
		yes->setText(obs_module_text(
			"AdvSceneSwitcher.condition.video.askFileAction.file"));
		auto no = msgBox.button(QMessageBox::StandardButton::No);
		no->setText(obs_module_text(
			"AdvSceneSwitcher.condition.video.askFileAction.screenshot"));
#endif
		msgBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint |
				      Qt::CustomizeWindowHint);
		const auto result = msgBox.exec();
		if (result == QMessageBox::Cancel) {
			return;
		}
		useExistingFile = result == QMessageBox::Yes;
	}

	if (useExistingFile) {
		path = QFileDialog::getOpenFileName(
			this, "",
			FileSelection::ValidPathOrDesktop(
				QString::fromStdString(_entryData->_file)));
		if (path.isEmpty()) {
			return;
		}

	} else {
		auto source = obs_weak_source_get_source(
			_entryData->_video.GetVideo());
		ScreenshotHelper screenshot(source);
		obs_source_release(source);

		path = QFileDialog::getSaveFileName(
			this, "",
			FileSelection::ValidPathOrDesktop(
				QString::fromStdString(_entryData->_file)),
			"*.png");
		if (path.isEmpty()) {
			return;
		}
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			return;
		}
		if (!screenshot.done) { // Screenshot usually completed by now
			std::this_thread::sleep_for(std::chrono::seconds(1));
		}
		if (!screenshot.done) {
			DisplayMessage(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotFail"));
			return;
		}
		if (_entryData->_areaParameters.enable) {
			screenshot.image = screenshot.image.copy(
				_entryData->_areaParameters.area.x,
				_entryData->_areaParameters.area.y,
				_entryData->_areaParameters.area.width,
				_entryData->_areaParameters.area.height);
		}
		screenshot.image.save(path);
	}
	_imagePath->SetPath(path);
	ImagePathChanged(path);
}

void MacroConditionVideoEdit::UsePatternForChangedCheckChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_patternMatchParameters.useForChangedCheck = value;
	SetWidgetVisibility();
}

void MacroConditionVideoEdit::PatternThresholdChanged(
	const DoubleVariable &value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_patternMatchParameters.threshold = value;
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ReduceLatencyChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_blockUntilScreenshotDone = value;
}

void MacroConditionVideoEdit::UseAlphaAsMaskChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_patternMatchParameters.useAlphaAsMask = value;
	_entryData->LoadImageFromFile();
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::PatternMatchModeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_patternMatchParameters.matchMode =
		static_cast<cv::TemplateMatchModes>(
			_patternMatchMode->itemData(idx).toInt());
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ThrottleEnableChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_throttleEnabled = value;
	_throttleCount->setEnabled(value);
}

void MacroConditionVideoEdit::ThrottleCountChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	auto lock = LockContext();
	_entryData->_throttleCount = value / GetIntervalValue();
}

void MacroConditionVideoEdit::ShowMatchClicked()
{
	_previewDialog.show();
	_previewDialog.raise();
	_previewDialog.activateWindow();
	_previewDialog.ShowMatch();
}

static bool needsShowMatch(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT || cond == VideoCondition::OCR;
}

static bool needsThrottleControls(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT ||
	       cond == VideoCondition::HAS_CHANGED ||
	       cond == VideoCondition::HAS_NOT_CHANGED;
}

static bool needsThreshold(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::HAS_CHANGED ||
	       cond == VideoCondition::HAS_NOT_CHANGED;
}

static bool patternControlIsOptional(VideoCondition cond)
{
	return cond == VideoCondition::HAS_CHANGED ||
	       cond == VideoCondition::HAS_NOT_CHANGED;
}

static bool needsAreaControls(VideoCondition cond)
{
	return cond != VideoCondition::NO_IMAGE;
}

void MacroConditionVideoEdit::SetWidgetVisibility()
{
	_sources->setVisible(_entryData->_video.type ==
			     VideoInput::Type::SOURCE);
	_scenes->setVisible(_entryData->_video.type == VideoInput::Type::SCENE);
	_imagePath->setVisible(requiresFileInput(_entryData->GetCondition()));
	_usePatternForChangedCheck->setVisible(
		patternControlIsOptional(_entryData->GetCondition()));
	_patternThreshold->setVisible(
		needsThreshold(_entryData->GetCondition()));
	_useAlphaAsMask->setVisible(_entryData->GetCondition() ==
				    VideoCondition::PATTERN);
	SetLayoutVisible(_patternMatchModeLayout,
			 _entryData->GetCondition() == VideoCondition::PATTERN);
	_brightness->setVisible(_entryData->GetCondition() ==
				VideoCondition::BRIGHTNESS);
	_showMatch->setVisible(needsShowMatch(_entryData->GetCondition()));
	_ocr->setVisible(_entryData->GetCondition() == VideoCondition::OCR);
	_objectDetect->setVisible(_entryData->GetCondition() ==
				  VideoCondition::OBJECT);
	_color->setVisible(_entryData->GetCondition() == VideoCondition::COLOR);
	SetLayoutVisible(_throttleControlLayout,
			 needsThrottleControls(_entryData->GetCondition()));
	_area->setVisible(needsAreaControls(_entryData->GetCondition()));

	if (_entryData->GetCondition() == VideoCondition::HAS_CHANGED ||
	    _entryData->GetCondition() == VideoCondition::HAS_NOT_CHANGED) {
		_patternThreshold->setVisible(
			_entryData->_patternMatchParameters.useForChangedCheck);
		SetLayoutVisible(
			_patternMatchModeLayout,
			_entryData->_patternMatchParameters.useForChangedCheck);
	}
	Resize();
}

void MacroConditionVideoEdit::Resize()
{
	adjustSize();
	updateGeometry();
}

void MacroConditionVideoEdit::SetupPreviewDialogParams()
{
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
	_previewDialog.ObjDetectParametersChanged(
		_entryData->_objMatchParameters);
	_previewDialog.OCRParametersChanged(_entryData->_ocrParameters);
	_previewDialog.VideoSelectionChanged(_entryData->_video);
	_previewDialog.AreaParametersChanged(_entryData->_areaParameters);
	_previewDialog.ConditionChanged(
		static_cast<int>(_entryData->GetCondition()));
}

void MacroConditionVideoEdit::UpdateEntryData()
{
	if (!_entryData) {
		return;
	}

	_videoInputTypes->setCurrentIndex(
		static_cast<int>(_entryData->_video.type));
	_scenes->SetScene(_entryData->_video.scene);
	_sources->SetSource(_entryData->_video.source);
	_condition->setCurrentIndex(
		static_cast<int>(_entryData->GetCondition()));
	_reduceLatency->setChecked(_entryData->_blockUntilScreenshotDone);
	_imagePath->SetPath(QString::fromStdString(_entryData->_file));
	_usePatternForChangedCheck->setChecked(
		_entryData->_patternMatchParameters.useForChangedCheck);
	_patternThreshold->SetDoubleValue(
		_entryData->_patternMatchParameters.threshold);
	_useAlphaAsMask->setChecked(
		_entryData->_patternMatchParameters.useAlphaAsMask);
	_patternMatchMode->setCurrentIndex(_patternMatchMode->findData(
		_entryData->_patternMatchParameters.matchMode));
	_throttleEnable->setChecked(_entryData->_throttleEnabled);
	_throttleCount->setValue(_entryData->_throttleCount *
				 GetIntervalValue());
	UpdatePreviewTooltip();
	SetupPreviewDialogParams();
	SetWidgetVisibility();
}

} // namespace advss
