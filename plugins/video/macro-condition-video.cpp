#include "macro-condition-video.hpp"
#include "screenshot-dialog.hpp"

#include <layout-helpers.hpp>
#include <macro-condition-edit.hpp>
#include <plugin-state-helpers.hpp>
#include <QBuffer>
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

MacroConditionVideo::MacroConditionVideo(Macro *m)
	: QObject(),
	  MacroCondition(m, true)
{
}

void MacroConditionVideo::UpdateActiveKeeper()
{
	_activeKeeper.SetActive(_keepActive);
	if (_video.type == VideoInput::Type::OBS_MAIN_OUTPUT) {
		return;
	}
	if (!_keepActive) {
		_lastActiveKeeperSource = nullptr;
		return;
	}
	auto videoSource = _video.GetVideo();
	if (videoSource == _lastActiveKeeperSource) {
		return;
	}
	_lastActiveKeeperSource = videoSource;
	_activeKeeper.SetSource(OBSGetStrongRef(videoSource));
}

bool MacroConditionVideo::CheckCondition()
{
	if (!_video.ValidSelection()) {
		return false;
	}

	UpdateActiveKeeper();

	bool match = false;
	if (CheckShouldBeSkipped()) {
		return _lastMatchResult;
	}

	if (!FileInputIsUpToDate()) {
		LoadImageFromFile();
	}

	if (_blockUntilScreenshotDone) {
		GetScreenshot(true);
	}

	if (_screenshotData.IsDone()) {
		match = Compare();
		_lastMatchResult = match;

		if (!requiresFileInput(_condition)) {
			_matchImage = std::move(_screenshotData.GetImage());
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
	obs_data_set_bool(obj, "keepActive", _keepActive);
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
	_keepActive = obs_data_get_bool(obj, "keepActive");
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

	return true;
}

std::string MacroConditionVideo::GetShortDesc() const
{
	return _video.ToString();
}

void MacroConditionVideo::GetScreenshot(bool blocking)
{
	auto source = obs_weak_source_get_source(_video.GetVideo());
	_screenshotData.~Screenshot();
	QRect screenshotArea;
	if (_areaParameters.enable && _condition != VideoCondition::NO_IMAGE) {
		screenshotArea.setRect(_areaParameters.area.x,
				       _areaParameters.area.y,
				       _areaParameters.area.width,
				       _areaParameters.area.height);
	}
	const int timeout = GetIntervalValue() < 300 ? 300 : GetIntervalValue();
	new (&_screenshotData)
		Screenshot(source, screenshotArea, blocking, timeout);
	obs_source_release(source);
	_getNextScreenshot = false;
}

bool MacroConditionVideo::LoadImageFromFile()
{
	const QFileInfo info(QString::fromStdString(_file));
	_loadedFileLastModified = info.lastModified();
	_loadedFile = _file;
	if (!_matchImage.load(info.absoluteFilePath())) {
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

	emit InputFileChanged();
	return true;
}

void MacroConditionVideo::SetPageSegMode(tesseract::PageSegMode mode)
{
	_ocrParameters.SetPageMode(mode);
}

bool MacroConditionVideo::SetLanguageCode(const std::string &language)
{
	return _ocrParameters.SetLanguageCode(language);
}

bool MacroConditionVideo::SetTesseractBaseDir(const std::string &dir)
{
	return _ocrParameters.SetTesseractBasePath(dir);
}

void MacroConditionVideo::SetCondition(VideoCondition condition)
{
	_condition = condition;
	SetupTempVars();
}

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	double bestMatchValue =
		MatchPattern(_screenshotData.GetImage(), _patternImageData,
			     _patternMatchParameters.threshold, result,
			     _patternMatchParameters.useAlphaAsMask,
			     _patternMatchParameters.matchMode);

	if (result.total() == 0) {
		SetTempVarValue("similarity", std::to_string(bestMatchValue));
		SetTempVarValue("patternCount", "0");
		SetTempVarValue("matchX", "-1");
		SetTempVarValue("matchY", "-1");
		SetTempVarValue("matchWidth", "0");
		SetTempVarValue("matchHeight", "0");
		return false;
	}

	SetTempVarValue("similarity", std::to_string(bestMatchValue));
	SetTempVarValue("matchWidth",
			std::to_string(_patternImageData.rgbaPattern.cols));
	SetTempVarValue("matchHeight",
			std::to_string(_patternImageData.rgbaPattern.rows));

	if (IsTempVarInUse("matchX") || IsTempVarInUse("matchY")) {
		double maxVal;
		cv::Point maxLoc;
		cv::minMaxLoc(result, nullptr, &maxVal, nullptr, &maxLoc);
		if (maxVal > 0.0) {
			int matchX = maxLoc.x;
			int matchY = maxLoc.y;
			if (_areaParameters.enable) {
				matchX += _areaParameters.area.x;
				matchY += _areaParameters.area.y;
			}
			SetTempVarValue("matchX", std::to_string(matchX));
			SetTempVarValue("matchY", std::to_string(matchY));
		} else {
			SetTempVarValue("matchX", "-1");
			SetTempVarValue("matchY", "-1");
		}
	}

	if (IsTempVarInUse("patternCount")) {
		const auto count = CountPatternMatches(
			result, {_patternImageData.rgbaPattern.cols,
				 _patternImageData.rgbaPattern.rows});
		SetTempVarValue("patternCount", std::to_string(count));
		return count > 0;
	}

	return countNonZero(result) > 0;
}

bool MacroConditionVideo::FileInputIsUpToDate() const
{
	if (!requiresFileInput(_condition)) {
		return true;
	}
	const QFileInfo info(QString::fromStdString(_file));
	return (_loadedFileLastModified == info.lastModified()) &&
	       (_file == _loadedFile);
}

bool MacroConditionVideo::OutputChanged()
{
	if (!_patternMatchParameters.useForChangedCheck) {
		return _screenshotData.GetImage() != _matchImage;
	}

	cv::Mat result;
	_patternImageData = CreatePatternData(_matchImage);
	double bestMatchValue =
		MatchPattern(_screenshotData.GetImage(), _patternImageData,
			     _patternMatchParameters.threshold, result,
			     _patternMatchParameters.useAlphaAsMask,
			     _patternMatchParameters.matchMode);
	SetTempVarValue("similarity", std::to_string(bestMatchValue));
	if (result.total() == 0) {
		return false;
	}
	return countNonZero(result) == 0;
}

bool MacroConditionVideo::ScreenshotContainsObject()
{
	auto model = _objMatchParameters.GetModel();
	if (!model) {
		return false;
	}
	auto objects = MatchObject(_screenshotData.GetImage(), *model,
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
	_currentBrightness =
		GetAvgBrightness(_screenshotData.GetImage()) / 255.;
	SetTempVarValue("brightness", std::to_string(_currentBrightness));
	return _currentBrightness > _brightnessThreshold;
}

bool MacroConditionVideo::CheckOCR()
{
	auto *ocr = _ocrParameters.GetOCR();
	if (!ocr) {
		return false;
	}

	auto text = RunOCR(ocr, _screenshotData.GetImage(),
			   _ocrParameters.color.GetValue(),
			   _ocrParameters.colorThreshold);
	if (!text) {
		return false;
	}

	SetVariableValue(*text);
	SetTempVarValue("text", *text);
	if (!_ocrParameters.regex.Enabled()) {
		return text == std::string(_ocrParameters.text);
	}
	return _ocrParameters.regex.Matches(*text, _ocrParameters.text);
}

bool MacroConditionVideo::CheckColor()
{
	const bool ret = ContainsPixelsInColorRange(
		_screenshotData.GetImage(), _colorParameters.color.GetValue(),
		_colorParameters.colorThreshold,
		_colorParameters.matchThreshold);

	SetTempVarValue("color", [&]() {
		return GetAverageColor(_screenshotData.GetImage())
			.name(QColor::HexArgb)
			.toStdString();
	});

	SetTempVarValue("dominantColor", [&]() {
		return GetDominantColor(_screenshotData.GetImage(), 3)
			.name(QColor::HexArgb)
			.toStdString();
	});

	return ret;
}

bool MacroConditionVideo::Compare()
{
	if (_condition != VideoCondition::OCR) {
		SetVariableValue("");
	}

	switch (_condition) {
	case VideoCondition::MATCH:
		return _screenshotData.GetImage() == _matchImage;
	case VideoCondition::DIFFER:
		return _screenshotData.GetImage() != _matchImage;
	case VideoCondition::HAS_CHANGED:
		return OutputChanged();
	case VideoCondition::HAS_NOT_CHANGED:
		return !OutputChanged();
	case VideoCondition::NO_IMAGE:
		return _screenshotData.GetImage().isNull();
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
	case VideoCondition::HAS_CHANGED:
	case VideoCondition::HAS_NOT_CHANGED:
		if (!_patternMatchParameters.useForChangedCheck) {
			break;
		}
		AddTempvar(
			"similarity",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.similarity"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.similarity.description"));
		break;
	case VideoCondition::PATTERN:
		AddTempvar(
			"similarity",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.similarity"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.similarity.description"));
		AddTempvar(
			"patternCount",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.patternCount"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.patternCount.description"));
		AddTempvar(
			"matchX",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchX"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchX.description"));
		AddTempvar(
			"matchY",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchY"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchY.description"));
		AddTempvar(
			"matchWidth",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchWidth"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchWidth.description"));
		AddTempvar(
			"matchHeight",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchHeight"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.matchHeight.description"));
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
		AddTempvar(
			"dominantColor",
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.dominantColor"),
			obs_module_text(
				"AdvSceneSwitcher.tempVar.video.dominantColor.description"));
		break;
	case VideoCondition::MATCH:
	case VideoCondition::DIFFER:
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
	for (auto &[value, name] : conditionTypes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(value));
	}
}

static inline void populatePatternMatchModeSelection(QComboBox *list)
{
	for (const auto &[mode, name] : patternMatchModes) {
		list->addItem(obs_module_text(name.c_str()),
			      static_cast<int>(mode));
	}
}

static QStringList getVideoSourcesList()
{
	auto sources = GetVideoSourceNames();
	sources.sort();
	return sources;
}

MacroConditionVideoEdit::MacroConditionVideoEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVideo> entryData)
	: QWidget(parent),
	  _videoInputTypes(new QComboBox()),
	  _scenes(new SceneSelectionWidget(this, true, false, true, true,
					   true)),
	  _sources(new SourceSelectionWidget(this, getVideoSourcesList, true)),
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
	  _throttleCount(new QSpinBox()),
	  _keepActive(new QCheckBox(
		  obs_module_text("AdvSceneSwitcher.keepSourceActive"))),
	  _keepActiveHelp(new HelpIcon(
		  obs_module_text("AdvSceneSwitcher.keepSourceActive.help"),
		  this))
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
	QWidget::connect(_keepActive, SIGNAL(stateChanged(int)), this,
			 SLOT(KeepActiveChanged(int)));
	QWidget::connect(_showMatch, SIGNAL(clicked()), this,
			 SLOT(ShowMatchClicked()));
	QWidget::connect(this,
			 SIGNAL(VideoSelectionChanged(const VideoInput &)),
			 &_previewDialog,
			 SLOT(VideoSelectionChanged(const VideoInput &)));
	QWidget::connect(_area, SIGNAL(Resized()), this, SLOT(Resize()));
	QWidget::connect(entryData.get(),
			 &MacroConditionVideo::InputFileChanged, this,
			 [this]() {
				 UpdatePreviewTooltip();
				 _previewDialog.PatternMatchParametersChanged(
					 _entryData->_patternMatchParameters);
			 });

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
	PlaceWidgets(obs_module_text("AdvSceneSwitcher.condition.video.layout"),
		     entryLine1Layout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.patternMatchMode"),
		_patternMatchModeLayout, widgetPlaceholders);
	PlaceWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.layout.throttle"),
		_throttleControlLayout, widgetPlaceholders);

	QHBoxLayout *keepActiveLayout = new QHBoxLayout;
	keepActiveLayout->addWidget(_keepActive);
	keepActiveLayout->addWidget(_keepActiveHelp);
	keepActiveLayout->addStretch();

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
	mainLayout->addLayout(keepActiveLayout);
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
	GUARD_LOADING_AND_LOCK();
	_entryData->_video.source = source;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::SceneChanged(const SceneSelection &scene)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_video.scene = scene;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::VideoInputTypeChanged(int type)
{
	GUARD_LOADING_AND_LOCK();
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

void MacroConditionVideoEdit::ConditionChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->SetCondition(
		static_cast<VideoCondition>(_condition->itemData(idx).toInt()));
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

	SetupPreviewDialogParams();
}

void MacroConditionVideoEdit::ImagePathChanged(const QString &text)
{
	GUARD_LOADING_AND_LOCK();
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
		auto yes = msgBox.button(QMessageBox::StandardButton::Yes);
		yes->setText(obs_module_text(
			"AdvSceneSwitcher.condition.video.askFileAction.file"));
		auto no = msgBox.button(QMessageBox::StandardButton::No);
		no->setText(obs_module_text(
			"AdvSceneSwitcher.condition.video.askFileAction.screenshot"));
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

		auto screenshot = ScreenshotDialog::AskForScreenshot(
			_entryData->_video, _entryData->_areaParameters);
		if (!screenshot) {
			return;
		}
		screenshot->save(path);
	}
	_imagePath->SetPath(path);
	ImagePathChanged(path);
}

void MacroConditionVideoEdit::UsePatternForChangedCheckChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_patternMatchParameters.useForChangedCheck = value;
	_entryData->SetupTempVars();
	SetWidgetVisibility();
}

void MacroConditionVideoEdit::PatternThresholdChanged(
	const DoubleVariable &value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_patternMatchParameters.threshold = value;
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ReduceLatencyChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_blockUntilScreenshotDone = value;
}

void MacroConditionVideoEdit::UseAlphaAsMaskChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_patternMatchParameters.useAlphaAsMask = value;
	_entryData->LoadImageFromFile();
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::PatternMatchModeChanged(int idx)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_patternMatchParameters.matchMode =
		static_cast<cv::TemplateMatchModes>(
			_patternMatchMode->itemData(idx).toInt());
	_previewDialog.PatternMatchParametersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ThrottleEnableChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_throttleEnabled = value;
	_throttleCount->setEnabled(value);
}

void MacroConditionVideoEdit::ThrottleCountChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_throttleCount = value / GetIntervalValue();
}

void MacroConditionVideoEdit::KeepActiveChanged(int value)
{
	GUARD_LOADING_AND_LOCK();
	_entryData->_keepActive = value;
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

	const bool sourceOrScene = _entryData->_video.type !=
				   VideoInput::Type::OBS_MAIN_OUTPUT;
	_keepActive->setVisible(sourceOrScene);
	_keepActiveHelp->setVisible(sourceOrScene);

	if (_entryData->GetCondition() == VideoCondition::HAS_CHANGED ||
	    _entryData->GetCondition() == VideoCondition::HAS_NOT_CHANGED) {
		_patternThreshold->setVisible(
			_entryData->_patternMatchParameters.useForChangedCheck);
		SetLayoutVisible(
			_patternMatchModeLayout,
			_entryData->_patternMatchParameters.useForChangedCheck);
	}

	// TODO:
	// Remove "reduce matching latency" and "reduce cpu load" options as they
	// have become superfluous with short circuit evaluation
	if (!_entryData->_throttleEnabled) {
		SetLayoutVisible(_throttleControlLayout, false);
	}
	if (_entryData->_blockUntilScreenshotDone) {
		_reduceLatency->hide();
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
	_condition->setCurrentIndex(_condition->findData(
		static_cast<int>(_entryData->GetCondition())));
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
	_keepActive->setChecked(_entryData->_keepActive);
	UpdatePreviewTooltip();
	SetupPreviewDialogParams();
	SetWidgetVisibility();
}

} // namespace advss
