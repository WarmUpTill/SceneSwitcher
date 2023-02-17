#include "macro-condition-video.hpp"

#include <advanced-scene-switcher.hpp>
#include <macro-condition-edit.hpp>
#include <switcher-data-structs.hpp>
#include <utility.hpp>

#include <QFileDialog>
#include <QBuffer>
#include <QToolTip>
#include <QMessageBox>
#include <QtGlobal>
#include <QColorDialog>

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
	obs_data_set_double(obj, "brightness", _brightnessThreshold);
	_patternMatchParameters.Save(obj);
	_objMatchParameters.Save(obj);
	_ocrParamters.Save(obj);
	obs_data_set_bool(obj, "throttleEnabled", _throttleEnabled);
	obs_data_set_int(obj, "throttleCount", _throttleCount);
	_areaParameters.Save(obj);
	return true;
}

bool MacroConditionVideo::Load(obs_data_t *obj)
{
	MacroCondition::Load(obj);
	_video.Load(obj);
	_condition =
		static_cast<VideoCondition>(obs_data_get_int(obj, "condition"));
	_file = obs_data_get_string(obj, "filePath");
	_blockUntilScreenshotDone =
		obs_data_get_bool(obj, "blockUntilScreenshotDone");
	_brightnessThreshold = obs_data_get_double(obj, "brightness");
	_patternMatchParameters.Load(obj);
	_objMatchParameters.Load(obj);
	_ocrParamters.Load(obj);
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
	new (&_screenshotData)
		ScreenshotHelper(source, blocking, GetSwitcher()->interval);
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
	_patternImageData = createPatternData(_matchImage);
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
	_ocrParamters.SetPageMode(mode);
}

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	matchPattern(_screenshotData.image, _patternImageData,
		     _patternMatchParameters.threshold, result,
		     _patternMatchParameters.useAlphaAsMask,
		     _patternMatchParameters.matchMode);
	return countNonZero(result) > 0;
}

bool MacroConditionVideo::OutputChanged()
{
	if (_patternMatchParameters.useForChangedCheck) {
		cv::Mat result;
		_patternImageData = createPatternData(_matchImage);
		matchPattern(_screenshotData.image, _patternImageData,
			     _patternMatchParameters.threshold, result,
			     _patternMatchParameters.useAlphaAsMask,
			     _patternMatchParameters.matchMode);
		return countNonZero(result) == 0;
	}
	return _screenshotData.image != _matchImage;
}

bool MacroConditionVideo::ScreenshotContainsObject()
{
	auto objects = matchObject(_screenshotData.image,
				   _objMatchParameters.cascade,
				   _objMatchParameters.scaleFactor,
				   _objMatchParameters.minNeighbors,
				   _objMatchParameters.minSize.CV(),
				   _objMatchParameters.maxSize.CV());
	return objects.size() > 0;
}

bool MacroConditionVideo::CheckBrightnessThreshold()
{
	_currentBrightness = getAvgBrightness(_screenshotData.image) / 255.;
	return _currentBrightness > _brightnessThreshold;
}

bool MacroConditionVideo::CheckOCR()
{
	if (!_ocrParamters.Initialized()) {
		return false;
	}

	auto text = runOCR(_ocrParamters.GetOCR(), _screenshotData.image,
			   _ocrParamters.color);

	if (_ocrParamters.regex.Enabled()) {
		auto expr = _ocrParamters.regex.GetRegularExpression(
			_ocrParamters.text);
		if (!expr.isValid()) {
			return false;
		}
		auto match = expr.match(QString::fromStdString(text));
		return match.hasMatch();
	}

	SetVariableValue(text);
	return text == std::string(_ocrParamters.text);
}

bool MacroConditionVideo::Compare()
{
	if (_areaParameters.enable && _condition != VideoCondition::NO_IMAGE) {
		_screenshotData.image = _screenshotData.image.copy(
			_areaParameters.area.x, _areaParameters.area.y,
			_areaParameters.area.width,
			_areaParameters.area.height);
	}

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
	default:
		break;
	}
	return false;
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
	  _brightnessThreshold(new SliderSpinBox(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.brightnessThresholdDescription"))),
	  _currentBrightness(new QLabel),
	  _ocrLayout(new QVBoxLayout),
	  _matchText(new VariableTextEdit(this)),
	  _regex(new RegexConfigWidget(this)),
	  _textColor(new QLabel),
	  _selectColor(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectColor"))),
	  _pageSegMode(new QComboBox()),
	  _modelDataPath(new FileSelection()),
	  _modelPathLayout(new QHBoxLayout),
	  _objectScaleThreshold(new SliderSpinBox(
		  1.1, 5.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.objectScaleThresholdDescription"))),
	  _neighborsControlLayout(new QHBoxLayout),
	  _minNeighbors(new QSpinBox()),
	  _minNeighborsDescription(new QLabel(obs_module_text(
		  "AdvSceneSwitcher.condition.video.minNeighborDescription"))),
	  _sizeLayout(new QHBoxLayout()),
	  _minSize(new SizeSelection(0, 1024)),
	  _maxSize(new SizeSelection(0, 4096)),
	  _checkAreaControlLayout(new QHBoxLayout),
	  _checkAreaEnable(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.entry.checkAreaEnable"))),
	  _checkArea(new AreaSelection(0, 99999)),
	  _selectArea(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectArea"))),
	  _throttleControlLayout(new QHBoxLayout),
	  _throttleEnable(new QCheckBox()),
	  _throttleCount(new QSpinBox()),
	  _showMatch(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.showMatch"))),
	  _previewDialog(this)
{
	_reduceLatency->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.reduceLatency.tooltip"));
	_imagePath->Button()->disconnect();
	_usePatternForChangedCheck->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.usePatternForChangedCheck.tooltip"));
	_patternMatchMode->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.patternMatchMode.tip"));
	populatePatternMatchModeSelection(_patternMatchMode);
	_minNeighbors->setMinimum(minMinNeighbors);
	_minNeighbors->setMaximum(maxMinNeighbors);
	populatePageSegModeSelection(_pageSegMode);
	_throttleCount->setMinimum(1 * GetSwitcher()->interval);
	_throttleCount->setMaximum(10 * GetSwitcher()->interval);
	_throttleCount->setSingleStep(GetSwitcher()->interval);

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
	QWidget::connect(_patternThreshold, SIGNAL(DoubleValueChanged(double)),
			 this, SLOT(PatternThresholdChanged(double)));
	QWidget::connect(_useAlphaAsMask, SIGNAL(stateChanged(int)), this,
			 SLOT(UseAlphaAsMaskChanged(int)));
	QWidget::connect(_patternMatchMode, SIGNAL(currentIndexChanged(int)),
			 this, SLOT(PatternMatchModeChanged(int)));
	QWidget::connect(_brightnessThreshold,
			 SIGNAL(DoubleValueChanged(double)), this,
			 SLOT(BrightnessThresholdChanged(double)));
	QWidget::connect(_objectScaleThreshold,
			 SIGNAL(DoubleValueChanged(double)), this,
			 SLOT(ObjectScaleThresholdChanged(double)));
	QWidget::connect(_minNeighbors, SIGNAL(valueChanged(int)), this,
			 SLOT(MinNeighborsChanged(int)));
	QWidget::connect(_minSize, SIGNAL(SizeChanged(advss::Size)), this,
			 SLOT(MinSizeChanged(advss::Size)));
	QWidget::connect(_maxSize, SIGNAL(SizeChanged(advss::Size)), this,
			 SLOT(MaxSizeChanged(advss::Size)));
	QWidget::connect(_checkAreaEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(CheckAreaEnableChanged(int)));
	QWidget::connect(_checkArea, SIGNAL(AreaChanged(advss::Area)), this,
			 SLOT(CheckAreaChanged(advss::Area)));
	QWidget::connect(_modelDataPath, SIGNAL(PathChanged(const QString &)),
			 this, SLOT(ModelPathChanged(const QString &)));
	QWidget::connect(_throttleEnable, SIGNAL(stateChanged(int)), this,
			 SLOT(ThrottleEnableChanged(int)));
	QWidget::connect(_throttleCount, SIGNAL(valueChanged(int)), this,
			 SLOT(ThrottleCountChanged(int)));
	QWidget::connect(_showMatch, SIGNAL(clicked()), this,
			 SLOT(ShowMatchClicked()));
	QWidget::connect(&_previewDialog, SIGNAL(SelectionAreaChanged(QRect)),
			 this, SLOT(CheckAreaChanged(QRect)));
	QWidget::connect(this,
			 SIGNAL(VideoSelectionChanged(const VideoInput &)),
			 &_previewDialog,
			 SLOT(VideoSelectionChanged(const VideoInput &)));
	QWidget::connect(_condition, SIGNAL(currentIndexChanged(int)),
			 &_previewDialog, SLOT(ConditionChanged(int)));
	QWidget::connect(_selectArea, SIGNAL(clicked()), this,
			 SLOT(SelectAreaClicked()));
	QWidget::connect(_selectColor, SIGNAL(clicked()), this,
			 SLOT(SelectColorClicked()));
	QWidget::connect(_matchText, SIGNAL(textChanged()), this,
			 SLOT(MatchTextChanged()));
	QWidget::connect(_regex, SIGNAL(RegexConfigChanged(RegexConfig)), this,
			 SLOT(RegexChanged(RegexConfig)));
	QWidget::connect(_pageSegMode, SIGNAL(currentIndexChanged(int)), this,
			 SLOT(PageSegModeChanged(int)));

	populateVideoInputSelection(_videoInputTypes);
	populateConditionSelection(_condition);

	_patternMatchModeLayout->setContentsMargins(0, 0, 0, 0);
	_ocrLayout->setContentsMargins(0, 0, 0, 0);
	_modelPathLayout->setContentsMargins(0, 0, 0, 0);
	_neighborsControlLayout->setContentsMargins(0, 0, 0, 0);
	_sizeLayout->setContentsMargins(0, 0, 0, 0);
	_checkAreaControlLayout->setContentsMargins(0, 0, 0, 0);
	_throttleControlLayout->setContentsMargins(0, 0, 0, 0);

	QHBoxLayout *entryLine1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoInputTypes}}", _videoInputTypes},
		{"{{sources}}", _sources},
		{"{{scenes}}", _scenes},
		{"{{condition}}", _condition},
		{"{{reduceLatency}}", _reduceLatency},
		{"{{imagePath}}", _imagePath},
		{"{{minNeighbors}}", _minNeighbors},
		{"{{minSize}}", _minSize},
		{"{{maxSize}}", _maxSize},
		{"{{modelDataPath}}", _modelDataPath},
		{"{{throttleEnable}}", _throttleEnable},
		{"{{throttleCount}}", _throttleCount},
		{"{{checkAreaEnable}}", _checkAreaEnable},
		{"{{checkArea}}", _checkArea},
		{"{{selectArea}}", _selectArea},
		{"{{textColor}}", _textColor},
		{"{{selectColor}}", _selectColor},
		{"{{textType}}", _pageSegMode},
		{"{{patternMatchingModes}}", _patternMatchMode},
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     entryLine1Layout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.patternMatchMode"),
		_patternMatchModeLayout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.modelPath"),
		_modelPathLayout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.minNeighbor"),
		_neighborsControlLayout, widgetPlaceholders);
	placeWidgets(obs_module_text(
			     "AdvSceneSwitcher.condition.video.entry.throttle"),
		     _throttleControlLayout, widgetPlaceholders);
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.checkArea"),
		_checkAreaControlLayout, widgetPlaceholders);

	_ocrLayout->addWidget(_matchText);
	auto regexLayout = new QHBoxLayout;
	regexLayout->addWidget(_regex);
	regexLayout->addStretch();
	_ocrLayout->addLayout(regexLayout);
	auto pageModeSegLayout = new QHBoxLayout();
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.orcTextType"),
		pageModeSegLayout, widgetPlaceholders);
	_ocrLayout->addLayout(pageModeSegLayout);
	auto colorPickLayout = new QHBoxLayout();
	placeWidgets(
		obs_module_text(
			"AdvSceneSwitcher.condition.video.entry.orcColorPick"),
		colorPickLayout, widgetPlaceholders);
	_ocrLayout->addLayout(colorPickLayout);

	QGridLayout *sizeGrid = new QGridLayout;
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
	_sizeLayout->addLayout(sizeGrid);
	_sizeLayout->addStretch();

	QHBoxLayout *showMatchLayout = new QHBoxLayout;
	showMatchLayout->addWidget(_showMatch);
	showMatchLayout->addStretch();
	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(entryLine1Layout);
	mainLayout->addWidget(_usePatternForChangedCheck);
	mainLayout->addWidget(_patternThreshold);
	mainLayout->addWidget(_useAlphaAsMask);
	mainLayout->addLayout(_patternMatchModeLayout);
	mainLayout->addWidget(_brightnessThreshold);
	mainLayout->addWidget(_currentBrightness);
	mainLayout->addLayout(_ocrLayout);
	mainLayout->addLayout(_modelPathLayout);
	mainLayout->addWidget(_objectScaleThreshold);
	mainLayout->addLayout(_neighborsControlLayout);
	mainLayout->addWidget(_minNeighborsDescription);
	mainLayout->addLayout(_sizeLayout);
	mainLayout->addLayout(_throttleControlLayout);
	mainLayout->addLayout(_checkAreaControlLayout);
	mainLayout->addWidget(_reduceLatency);
	mainLayout->addLayout(showMatchLayout);
	setLayout(mainLayout);

	_entryData = entryData;
	UpdateEntryData();
	_loading = false;

	connect(&_updateBrightnessTimer, &QTimer::timeout, this,
		&MacroConditionVideoEdit::UpdateCurrentBrightness);
	_updateBrightnessTimer.start(1000);
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

void MacroConditionVideoEdit::SourceChanged(const SourceSelection &source)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_video.source = source;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::SceneChanged(const SceneSelection &scene)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_video.scene = scene;
	HandleVideoInputUpdate();
}

void MacroConditionVideoEdit::VideoInputTypeChanged(int type)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_video.type = static_cast<VideoInput::Type>(type);
	HandleVideoInputUpdate();
	SetWidgetVisibility();
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
	_previewDialog.PatternMatchParamtersChanged(
		_entryData->_patternMatchParameters);

	if (_entryData->_condition == VideoCondition::OBJECT) {
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

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_file = text.toUtf8().constData();
	_entryData->ResetLastMatch();
	if (_entryData->LoadImageFromFile()) {
		UpdatePreviewTooltip();
	}
	_previewDialog.PatternMatchParamtersChanged(
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

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_patternMatchParameters.useForChangedCheck = value;
	SetWidgetVisibility();
}

void MacroConditionVideoEdit::PatternThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_patternMatchParameters.threshold = value;
	_previewDialog.PatternMatchParamtersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::ReduceLatencyChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_blockUntilScreenshotDone = value;
}

void MacroConditionVideoEdit::UseAlphaAsMaskChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_patternMatchParameters.useAlphaAsMask = value;
	_entryData->LoadImageFromFile();
	_previewDialog.PatternMatchParamtersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::PatternMatchModeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_patternMatchParameters.matchMode =
		static_cast<cv::TemplateMatchModes>(
			_patternMatchMode->itemData(idx).toInt());
	_previewDialog.PatternMatchParamtersChanged(
		_entryData->_patternMatchParameters);
}

void MacroConditionVideoEdit::BrightnessThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_brightnessThreshold = value;
}

void MacroConditionVideoEdit::ObjectScaleThresholdChanged(double value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_objMatchParameters.scaleFactor = value;
	_previewDialog.ObjDetectParamtersChanged(
		_entryData->_objMatchParameters);
}

void MacroConditionVideoEdit::MinNeighborsChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_objMatchParameters.minNeighbors = value;
	_previewDialog.ObjDetectParamtersChanged(
		_entryData->_objMatchParameters);
}

void MacroConditionVideoEdit::MinSizeChanged(advss::Size value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_objMatchParameters.minSize = value;
	_previewDialog.ObjDetectParamtersChanged(
		_entryData->_objMatchParameters);
}

void MacroConditionVideoEdit::HandleVideoInputUpdate()
{
	_entryData->ResetLastMatch();
	emit HeaderInfoChanged(
		QString::fromStdString(_entryData->GetShortDesc()));
	emit VideoSelectionChanged(_entryData->_video);
}

void MacroConditionVideoEdit::MaxSizeChanged(advss::Size value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_objMatchParameters.maxSize = value;
	_previewDialog.ObjDetectParamtersChanged(
		_entryData->_objMatchParameters);
}

void MacroConditionVideoEdit::SetupColorLabel(const QColor &color)
{
	_textColor->setText(color.name());
	_textColor->setPalette(QPalette(color));
	_textColor->setAutoFillBackground(true);
}

void MacroConditionVideoEdit::SelectColorClicked()
{
	if (_loading || !_entryData) {
		return;
	}

	const QColor color = QColorDialog::getColor(
		_entryData->_ocrParamters.color, this,
		obs_module_text("AdvSceneSwitcher.condition.video.selectColor"),
		QColorDialog::ColorDialogOption());

	if (!color.isValid()) {
		return;
	}

	SetupColorLabel(color);
	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_ocrParamters.color = color;

	_previewDialog.OCRParamtersChanged(_entryData->_ocrParamters);
}

void MacroConditionVideoEdit::MatchTextChanged()
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_ocrParamters.text =
		_matchText->toPlainText().toUtf8().constData();

	adjustSize();
	updateGeometry();

	_previewDialog.OCRParamtersChanged(_entryData->_ocrParamters);
}

void MacroConditionVideoEdit::RegexChanged(RegexConfig conf)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_ocrParamters.regex = conf;
	adjustSize();
	updateGeometry();

	_previewDialog.OCRParamtersChanged(_entryData->_ocrParamters);
}

void MacroConditionVideoEdit::PageSegModeChanged(int idx)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->SetPageSegMode(static_cast<tesseract::PageSegMode>(
		_pageSegMode->itemData(idx).toInt()));

	_previewDialog.OCRParamtersChanged(_entryData->_ocrParamters);
}

void MacroConditionVideoEdit::CheckAreaEnableChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_areaParameters.enable = value;
	_checkArea->setEnabled(value);
	_selectArea->setEnabled(value);
	_checkArea->setVisible(value);
	_selectArea->setVisible(value);
	adjustSize();
	_previewDialog.AreaParamtersChanged(_entryData->_areaParameters);
}

void MacroConditionVideoEdit::CheckAreaChanged(advss::Area value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_areaParameters.area = value;
	_previewDialog.AreaParamtersChanged(_entryData->_areaParameters);
}

void MacroConditionVideoEdit::CheckAreaChanged(QRect rect)
{
	advss::Area area{rect.topLeft().x(), rect.y(), rect.width(),
			 rect.height()};
	_checkArea->SetArea(area);
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

void MacroConditionVideoEdit::ShowMatchClicked()
{
	_previewDialog.show();
	_previewDialog.raise();
	_previewDialog.activateWindow();
	_previewDialog.ShowMatch();
}

void MacroConditionVideoEdit::UpdateCurrentBrightness()
{
	QString text = obs_module_text(
		"AdvSceneSwitcher.condition.video.currentBrightness");
	_currentBrightness->setText(
		text.arg(_entryData->GetCurrentBrightness()));
}

void MacroConditionVideoEdit::SelectAreaClicked()
{
	_previewDialog.show();
	_previewDialog.raise();
	_previewDialog.activateWindow();
	_previewDialog.SelectArea();
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

static bool needsShowMatch(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT || cond == VideoCondition::OCR;
}

static bool needsObjectControls(VideoCondition cond)
{
	return cond == VideoCondition::OBJECT;
}

static bool needsThrottleControls(VideoCondition cond)
{
	return cond == VideoCondition::PATTERN ||
	       cond == VideoCondition::OBJECT;
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
	_imagePath->setVisible(requiresFileInput(_entryData->_condition));
	_usePatternForChangedCheck->setVisible(
		patternControlIsOptional(_entryData->_condition));
	_patternThreshold->setVisible(needsThreshold(_entryData->_condition));
	_useAlphaAsMask->setVisible(_entryData->_condition ==
				    VideoCondition::PATTERN);
	setLayoutVisible(_patternMatchModeLayout,
			 _entryData->_condition == VideoCondition::PATTERN);
	_brightnessThreshold->setVisible(_entryData->_condition ==
					 VideoCondition::BRIGHTNESS);
	_currentBrightness->setVisible(_entryData->_condition ==
				       VideoCondition::BRIGHTNESS);
	_showMatch->setVisible(needsShowMatch(_entryData->_condition));
	_objectScaleThreshold->setVisible(
		needsObjectControls(_entryData->_condition));
	setLayoutVisible(_neighborsControlLayout,
			 needsObjectControls(_entryData->_condition));
	_minNeighborsDescription->setVisible(
		needsObjectControls(_entryData->_condition));
	setLayoutVisible(_ocrLayout,
			 _entryData->_condition == VideoCondition::OCR);
	setLayoutVisible(_sizeLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_modelPathLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_throttleControlLayout,
			 needsThrottleControls(_entryData->_condition));
	setLayoutVisible(_checkAreaControlLayout,
			 needsAreaControls(_entryData->_condition));
	_checkArea->setVisible(_entryData->_areaParameters.enable);
	_selectArea->setVisible(_entryData->_areaParameters.enable);

	if (_entryData->_condition == VideoCondition::HAS_CHANGED ||
	    _entryData->_condition == VideoCondition::HAS_NOT_CHANGED) {
		_patternThreshold->setVisible(
			_entryData->_patternMatchParameters.useForChangedCheck);
		setLayoutVisible(
			_patternMatchModeLayout,
			_entryData->_patternMatchParameters.useForChangedCheck);
	}

	adjustSize();
}

void MacroConditionVideoEdit::SetupPreviewDialogParams()
{
	_previewDialog.PatternMatchParamtersChanged(
		_entryData->_patternMatchParameters);
	_previewDialog.ObjDetectParamtersChanged(
		_entryData->_objMatchParameters);
	_previewDialog.OCRParamtersChanged(_entryData->_ocrParamters);
	_previewDialog.VideoSelectionChanged(_entryData->_video);
	_previewDialog.AreaParamtersChanged(_entryData->_areaParameters);
	_previewDialog.ConditionChanged(
		static_cast<int>(_entryData->_condition));
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
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
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
	_brightnessThreshold->SetDoubleValue(_entryData->_brightnessThreshold);
	_modelDataPath->SetPath(_entryData->GetModelDataPath().c_str());
	_objectScaleThreshold->SetDoubleValue(
		_entryData->_objMatchParameters.scaleFactor);
	_matchText->setPlainText(_entryData->_ocrParamters.text);
	_regex->SetRegexConfig(_entryData->_ocrParamters.regex);
	SetupColorLabel(_entryData->_ocrParamters.color);
	_pageSegMode->setCurrentIndex(_pageSegMode->findData(
		static_cast<int>(_entryData->_ocrParamters.GetPageMode())));
	_minNeighbors->setValue(_entryData->_objMatchParameters.minNeighbors);
	_minSize->SetSize(_entryData->_objMatchParameters.minSize);
	_maxSize->SetSize(_entryData->_objMatchParameters.maxSize);
	_throttleEnable->setChecked(_entryData->_throttleEnabled);
	_throttleCount->setValue(_entryData->_throttleCount *
				 GetSwitcher()->interval);
	_checkAreaEnable->setChecked(_entryData->_areaParameters.enable);
	_checkArea->SetArea(_entryData->_areaParameters.area);
	UpdatePreviewTooltip();
	SetupPreviewDialogParams();
	SetWidgetVisibility();
}
