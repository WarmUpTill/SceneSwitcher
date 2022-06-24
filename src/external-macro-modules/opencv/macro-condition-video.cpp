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
	if (!_video.ValidSelection()) {
		return false;
	}

	bool match = false;
	if (CheckShouldBeSkipped()) {
		return _lastMatchResult;
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

	if (_getNextScreenshot) {
		GetScreenshot();
	}
	return match;
}

bool MacroConditionVideo::Save(obs_data_t *obj)
{
	MacroCondition::Save(obj);
	_video.Save(obj);
	obs_data_set_int(obj, "condition", static_cast<int>(_condition));
	obs_data_set_string(obj, "filePath", _file.c_str());
	obs_data_set_bool(obj, "usePatternForChangedCheck",
			  _usePatternForChangedCheck);
	obs_data_set_double(obj, "threshold", _patternThreshold);
	obs_data_set_bool(obj, "useAlphaAsMask", _useAlphaAsMask);
	obs_data_set_string(obj, "modelDataPath", _modelDataPath.c_str());
	obs_data_set_double(obj, "scaleFactor", _scaleFactor);
	obs_data_set_int(obj, "minNeighbors", _minNeighbors);
	_minSize.Save(obj, "minSize");
	_maxSize.Save(obj, "maxSize");
	obs_data_set_bool(obj, "throttleEnabled", _throttleEnabled);
	obs_data_set_int(obj, "throttleCount", _throttleCount);
	obs_data_set_bool(obj, "checkAreaEnabled", _checkAreaEnable);
	_checkArea.Save(obj, "checkArea");
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
	_video.Load(obj);
	if (obs_data_has_user_value(obj, "videoSource")) {
		_video.Load(obj, "videoSource");
	}
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
	// TODO: Remove this fallback in future version
	if (obs_data_has_user_value(obj, "minSizeX")) {
		_minSize.width = obs_data_get_int(obj, "minSizeX");
		_minSize.height = obs_data_get_int(obj, "minSizeY");
		_maxSize.width = obs_data_get_int(obj, "maxSizeX");
		_maxSize.height = obs_data_get_int(obj, "maxSizeY");
	} else {
		_minSize.Load(obj, "minSize");
		_maxSize.Load(obj, "maxSize");
	}
	_throttleEnabled = obs_data_get_bool(obj, "throttleEnabled");
	_throttleCount = obs_data_get_int(obj, "throttleCount");
	_checkAreaEnable = obs_data_get_bool(obj, "checkAreaEnabled");
	_checkArea.Load(obj, "checkArea");
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
	return _video.ToString();
}

void MacroConditionVideo::GetScreenshot()
{
	auto source = obs_weak_source_get_source(_video.GetVideo());
	_screenshotData.~ScreenshotHelper();
	new (&_screenshotData) ScreenshotHelper(source);
	obs_source_release(source);
	_getNextScreenshot = false;
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

bool MacroConditionVideo::ScreenshotContainsPattern()
{
	cv::Mat result;
	matchPattern(_screenshotData.image, _patternData, _patternThreshold,
		     result, _useAlphaAsMask);
	return countNonZero(result) > 0;
}

bool MacroConditionVideo::OutputChanged()
{
	if (_usePatternForChangedCheck) {
		cv::Mat result;
		_patternData = createPatternData(_matchImage);
		matchPattern(_screenshotData.image, _patternData,
			     _patternThreshold, result, _useAlphaAsMask);
		return countNonZero(result) == 0;
	}
	return _screenshotData.image != _matchImage;
}

bool MacroConditionVideo::ScreenshotContainsObject()
{
	auto objects = matchObject(_screenshotData.image, _objectCascade,
				   _scaleFactor, _minNeighbors, _minSize.CV(),
				   _maxSize.CV());
	return objects.size() > 0;
}

bool MacroConditionVideo::Compare()
{
	if (_checkAreaEnable && _condition != VideoCondition::NO_IMAGE) {
		_screenshotData.image = _screenshotData.image.copy(
			_checkArea.x, _checkArea.y, _checkArea.width,
			_checkArea.height);
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

MacroConditionVideoEdit::MacroConditionVideoEdit(
	QWidget *parent, std::shared_ptr<MacroConditionVideo> entryData)
	: QWidget(parent),
	  _videoSelection(new VideoSelectionWidget(this)),
	  _condition(new QComboBox()),
	  _usePatternForChangedCheck(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.usePatternForChangedCheck"))),
	  _imagePath(new FileSelection()),
	  _patternThreshold(new ThresholdSlider(
		  0., 1.,
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.patternThreshold"),
		  obs_module_text(
			  "AdvSceneSwitcher.condition.video.patternThresholdDescription"))),
	  _useAlphaAsMask(new QCheckBox(obs_module_text(
		  "AdvSceneSwitcher.condition.video.patternThresholdUseAlphaAsMask"))),
	  _modelDataPath(new FileSelection()),
	  _modelPathLayout(new QHBoxLayout),
	  _objectScaleThreshold(new ThresholdSlider(
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
	  _checkAreaEnable(new QCheckBox()),
	  _checkArea(new AreaSelection(0, 99999)),
	  _selectArea(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.selectArea"))),
	  _throttleControlLayout(new QHBoxLayout),
	  _throttleEnable(new QCheckBox()),
	  _throttleCount(new QSpinBox()),
	  _showMatch(new QPushButton(obs_module_text(
		  "AdvSceneSwitcher.condition.video.showMatch"))),
	  _previewDialog(this, entryData.get(), &GetSwitcher()->m)
{
	_imagePath->Button()->disconnect();
	_usePatternForChangedCheck->setToolTip(obs_module_text(
		"AdvSceneSwitcher.condition.video.usePatternForChangedCheck.tooltip"));
	_minNeighbors->setMinimum(minMinNeighbors);
	_minNeighbors->setMaximum(maxMinNeighbors);
	_throttleCount->setMinimum(1 * GetSwitcher()->interval);
	_throttleCount->setMaximum(10 * GetSwitcher()->interval);
	_throttleCount->setSingleStep(GetSwitcher()->interval);

	QWidget::connect(_videoSelection,
			 SIGNAL(VideoSelectionChange(const VideoSelection &)),
			 this,
			 SLOT(VideoSelectionChanged(const VideoSelection &)));
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
	QWidget::connect(_selectArea, SIGNAL(clicked()), this,
			 SLOT(SelectAreaClicked()));

	populateConditionSelection(_condition);

	QHBoxLayout *entryLine1Layout = new QHBoxLayout;
	std::unordered_map<std::string, QWidget *> widgetPlaceholders = {
		{"{{videoSources}}", _videoSelection},
		{"{{condition}}", _condition},
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
	};
	placeWidgets(obs_module_text("AdvSceneSwitcher.condition.video.entry"),
		     entryLine1Layout, widgetPlaceholders);
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
	_sizeLayout->setContentsMargins(0, 0, 0, 0);

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
	mainLayout->addLayout(_sizeLayout);
	mainLayout->addLayout(showMatchLayout);
	mainLayout->addLayout(_throttleControlLayout);
	mainLayout->addLayout(_checkAreaControlLayout);
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

void MacroConditionVideoEdit::VideoSelectionChanged(const VideoSelection &v)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_video = v;
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
	if (_entryData->_video.ValidSelection()) {
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
		auto source = obs_weak_source_get_source(
			_entryData->_video.GetVideo());
		ScreenshotHelper screenshot(source);
		obs_source_release(source);

		path = QFileDialog::getSaveFileName(this, "", "", "*.png");
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
		if (_entryData->_checkAreaEnable) {
			screenshot.image = screenshot.image.copy(
				_entryData->_checkArea.x,
				_entryData->_checkArea.y,
				_entryData->_checkArea.width,
				_entryData->_checkArea.height);
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

void MacroConditionVideoEdit::MinSizeChanged(advss::Size value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_minSize = value;
}

void MacroConditionVideoEdit::MaxSizeChanged(advss::Size value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_maxSize = value;
}

void MacroConditionVideoEdit::CheckAreaEnableChanged(int value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_checkAreaEnable = value;
	_checkArea->setEnabled(value);
	_selectArea->setEnabled(value);
}

void MacroConditionVideoEdit::CheckAreaChanged(advss::Area value)
{
	if (_loading || !_entryData) {
		return;
	}

	std::lock_guard<std::mutex> lock(GetSwitcher()->m);
	_entryData->_checkArea = value;
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

bool needsAreaControls(VideoCondition cond)
{
	return cond != VideoCondition::NO_IMAGE;
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
	setLayoutVisible(_sizeLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_modelPathLayout,
			 needsObjectControls(_entryData->_condition));
	setLayoutVisible(_throttleControlLayout,
			 needsThrottleControls(_entryData->_condition));
	setLayoutVisible(_checkAreaControlLayout,
			 needsAreaControls(_entryData->_condition));

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

	_videoSelection->SetVideoSelection(_entryData->_video);
	_condition->setCurrentIndex(static_cast<int>(_entryData->_condition));
	_imagePath->SetPath(QString::fromStdString(_entryData->_file));
	_usePatternForChangedCheck->setChecked(
		_entryData->_usePatternForChangedCheck);
	_patternThreshold->SetDoubleValue(_entryData->_patternThreshold);
	_useAlphaAsMask->setChecked(_entryData->_useAlphaAsMask);
	_modelDataPath->SetPath(_entryData->GetModelDataPath().c_str());
	_objectScaleThreshold->SetDoubleValue(_entryData->_scaleFactor);
	_minNeighbors->setValue(_entryData->_minNeighbors);
	_minSize->SetSize(_entryData->_minSize);
	_maxSize->SetSize(_entryData->_maxSize);
	_throttleEnable->setChecked(_entryData->_throttleEnabled);
	_throttleCount->setValue(_entryData->_throttleCount *
				 GetSwitcher()->interval);
	_checkAreaEnable->setChecked(_entryData->_checkAreaEnable);
	_checkArea->SetArea(_entryData->_checkArea);
	UpdatePreviewTooltip();
	SetWidgetVisibility();
}
