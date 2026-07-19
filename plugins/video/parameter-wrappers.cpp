#include "parameter-wrappers.hpp"
#include "cascade-classifier-detector.hpp"
#include "dnn-detector.hpp" // guarded internally with HAVE_OPENCV_DNN
#include "log-helper.hpp"

#include <QFileInfo>
#include <source-helpers.hpp>

namespace advss {

bool PatternMatchParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "useForChangedCheck", useForChangedCheck);
	threshold.Save(data, "threshold");
	obs_data_set_bool(data, "useAlphaAsMask", useAlphaAsMask);
	obs_data_set_int(data, "matchMode", matchMode);
	obs_data_set_int(data, "version", 1);
	obs_data_set_obj(obj, "patternMatchData", data);
	obs_data_release(data);
	return true;
}

bool PatternMatchParameters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "patternMatchData")) {
		useForChangedCheck =
			obs_data_get_bool(obj, "usePatternForChangedCheck");
		threshold = obs_data_get_double(obj, "threshold");
		useAlphaAsMask = obs_data_get_bool(obj, "useAlphaAsMask");
		return true;
	}
	auto data = obs_data_get_obj(obj, "patternMatchData");
	useForChangedCheck = obs_data_get_bool(data, "useForChangedCheck");
	threshold.Load(data, "threshold");
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(data, "version")) {
		threshold = obs_data_get_double(data, "threshold");
	}
	useAlphaAsMask = obs_data_get_bool(data, "useAlphaAsMask");
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(data, "matchMode")) {
		matchMode = cv::TM_CCORR_NORMED;
	} else {
		matchMode = static_cast<cv::TemplateMatchModes>(
			obs_data_get_int(data, "matchMode"));
	}
	obs_data_release(data);
	return true;
}

static bool isScaleFactorValid(double scaleFactor)
{
	return scaleFactor > 1.;
}

static bool isMinNeighborsValid(int minNeighbors)
{
	return minNeighbors >= minMinNeighbors &&
	       minNeighbors <= maxMinNeighbors;
}

bool CascadeClassifierParameters::LoadModelData()
{
#if CV_VERSION_MAJOR < 5
	const auto path = QString::fromStdString(_modelPath);
	if (!QFileInfo(path).exists(path)) {
		_detector.reset();
		return false;
	}
	auto det = std::make_unique<CascadeClassifierDetector>();
	if (!det->Load(_modelPath)) {
		_detector.reset();
		return false;
	}
	_detector = std::move(det);
	return true;
#else
	return false;
#endif
}

bool CascadeClassifierParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "modelPath", _modelPath.c_str());
	scaleFactor.Save(data, "scaleFactor");
	obs_data_set_int(data, "minNeighbors", minNeighbors);
	minSize.Save(data, "minSize");
	maxSize.Save(data, "maxSize");
	obs_data_set_int(data, "version", 2);
	obs_data_set_obj(obj, "objectMatchData", data);
	obs_data_release(data);
	return true;
}

bool CascadeClassifierParameters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "patternMatchData")) {
		_modelPath = obs_data_get_string(obj, "modelDataPath");
		scaleFactor = obs_data_get_double(obj, "scaleFactor");
		if (!isScaleFactorValid(scaleFactor)) {
			scaleFactor = 1.1;
		}
		minNeighbors = obs_data_get_int(obj, "minNeighbors");
		if (!isMinNeighborsValid(minNeighbors)) {
			minNeighbors = minMinNeighbors;
		}
		minSize.Load(obj, "minSize");
		maxSize.Load(obj, "maxSize");
		return true;
	}
	auto data = obs_data_get_obj(obj, "objectMatchData");
	_modelPath = obs_data_get_string(data, "modelPath");
	scaleFactor.Load(data, "scaleFactor");
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(data, "version")) {
		scaleFactor = obs_data_get_double(data, "scaleFactor");
	} else if (obs_data_get_int(data, "version") == 1) {
#ifdef _WIN32
		// On Windows the plugin directory moved from
		// "Program Files/.../data/obs-plugins/advanced-scene-switcher"
		// to "ProgramData/.../plugins/advanced-scene-switcher/data",
		// which invalidates previously saved default model paths.
		const std::string oldPrefix =
			"../../data/obs-plugins/advanced-scene-switcher/res/cascadeClassifiers/";
		if (_modelPath.substr(0, oldPrefix.size()) == oldPrefix) {
			_modelPath = std::string(obs_get_module_data_path(
					     obs_current_module())) +
				     "/res/cascadeClassifiers/" +
				     _modelPath.substr(oldPrefix.size());
		}
#endif
	}
	if (scaleFactor.IsFixedType() && !isScaleFactorValid(scaleFactor)) {
		scaleFactor = 1.1;
	}
	minNeighbors = obs_data_get_int(data, "minNeighbors");
	if (!isMinNeighborsValid(minNeighbors)) {
		minNeighbors = minMinNeighbors;
	}
	minSize.Load(data, "minSize");
	maxSize.Load(data, "maxSize");
	obs_data_release(data);
	return true;
}

bool CascadeClassifierParameters::SetModelPath(const std::string &path)
{
	_modelPath = path;
	return LoadModelData();
}

ObjectDetector *CascadeClassifierParameters::GetDetector()
{
	if (!_detector || !_detector->IsLoaded()) {
		if (!LoadModelData()) {
			return nullptr;
		}
	}
#if CV_VERSION_MAJOR < 5
	auto *cascade =
		static_cast<CascadeClassifierDetector *>(_detector.get());
	cascade->scaleFactor = scaleFactor;
	cascade->minNeighbors = minNeighbors;
	cascade->minSize = minSize.CV();
	cascade->maxSize = maxSize.CV();
#endif
	return _detector.get();
}

bool DnnDetectParameters::LoadModelData()
{
#ifdef HAVE_OPENCV_DNN
	const auto path = QString::fromStdString(_modelPath);
	if (!QFileInfo(path).exists(path)) {
		_detector.reset();
		return false;
	}
	auto det = std::make_unique<DnnDetector>();
	if (!det->Load(_modelPath)) {
		_detector.reset();
		return false;
	}
	_detector = std::move(det);
	return true;
#else
	return false;
#endif
}

bool DnnDetectParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "modelPath", _modelPath.c_str());
	confidenceThreshold.Save(data, "confidenceThreshold");
	nmsThreshold.Save(data, "nmsThreshold");
	inputSize.Save(data, "inputSize");
	obs_data_set_double(data, "scaleFactor", scaleFactor);
	obs_data_set_double(data, "meanR", meanR);
	obs_data_set_double(data, "meanG", meanG);
	obs_data_set_double(data, "meanB", meanB);
	obs_data_set_bool(data, "swapRB", swapRB);
	obs_data_set_obj(obj, "dnnObjectMatchData", data);
	obs_data_release(data);
	return true;
}

bool DnnDetectParameters::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "dnnObjectMatchData");
	if (!data) {
		return true;
	}
	_modelPath = obs_data_get_string(data, "modelPath");
	confidenceThreshold.Load(data, "confidenceThreshold");
	nmsThreshold.Load(data, "nmsThreshold");
	inputSize.Load(data, "inputSize");
	obs_data_set_default_double(data, "scaleFactor", 1.0 / 127.5);
	obs_data_set_default_double(data, "meanR", 127.5);
	obs_data_set_default_double(data, "meanG", 127.5);
	obs_data_set_default_double(data, "meanB", 127.5);
	obs_data_set_default_bool(data, "swapRB", true);
	scaleFactor = obs_data_get_double(data, "scaleFactor");
	meanR = obs_data_get_double(data, "meanR");
	meanG = obs_data_get_double(data, "meanG");
	meanB = obs_data_get_double(data, "meanB");
	swapRB = obs_data_get_bool(data, "swapRB");
	obs_data_release(data);
	return true;
}

bool DnnDetectParameters::SetModelPath(const std::string &path)
{
	_modelPath = path;
	return LoadModelData();
}

ObjectDetector *DnnDetectParameters::GetDetector()
{
	if (!_detector || !_detector->IsLoaded()) {
		if (!LoadModelData()) {
			return nullptr;
		}
	}
#ifdef HAVE_OPENCV_DNN
	auto *dnn = static_cast<DnnDetector *>(_detector.get());
	dnn->confidenceThreshold = confidenceThreshold;
	dnn->nmsThreshold = nmsThreshold;
	auto sz = inputSize.CV();
	dnn->inputWidth = sz.width;
	dnn->inputHeight = sz.height;
	dnn->scaleFactor = scaleFactor;
	dnn->meanR = meanR;
	dnn->meanG = meanG;
	dnn->meanB = meanB;
	dnn->swapRB = swapRB;
#endif
	return _detector.get();
}

bool AreaParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_bool(data, "enabled", enable);
	area.Save(data, "area");
	obs_data_set_obj(obj, "areaData", data);
	obs_data_release(data);
	return true;
}

bool AreaParameters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "areaData")) {
		enable = obs_data_get_bool(obj, "checkAreaEnabled");
		area.Load(obj, "checkArea");
		return true;
	}
	auto data = obs_data_get_obj(obj, "areaData");
	enable = obs_data_get_bool(data, "enabled");
	area.Load(data, "area");
	obs_data_release(data);
	return true;
}

bool VideoInput::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_int(data, "type", static_cast<int>(type));
	source.Save(data);
	scene.Save(data);
	obs_data_set_obj(obj, "videoInputData", data);
	obs_data_release(data);
	return true;
}

bool VideoInput::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (obs_data_has_user_value(obj, "videoType")) {
		enum class VideoSelectionType {
			SOURCE,
			OBS_MAIN,
		};
		auto oldType = static_cast<VideoSelectionType>(
			obs_data_get_int(obj, "videoType"));
		if (oldType == VideoSelectionType::SOURCE) {
			type = Type::SOURCE;
			auto name = obs_data_get_string(obj, "video");
			source.SetSource(GetWeakSourceByName(name));
		} else {
			type = Type::OBS_MAIN_OUTPUT;
		}
		return true;
	}

	auto data = obs_data_get_obj(obj, "videoInputData");
	type = static_cast<Type>(obs_data_get_int(data, "type"));
	source.Load(data);
	scene.Load(data);
	obs_data_release(data);
	return true;
}

std::string VideoInput::ToString(bool resolve) const
{
	switch (type) {
	case VideoInput::Type::OBS_MAIN_OUTPUT:
		return obs_module_text("AdvSceneSwitcher.OBSVideoOutput");
	case VideoInput::Type::SOURCE:
		return source.ToString(resolve);
	case VideoInput::Type::SCENE:
		return scene.ToString(resolve);
	}
	return "";
}

bool VideoInput::ValidSelection() const
{
	switch (type) {
	case VideoInput::Type::OBS_MAIN_OUTPUT:
		return true;
	case VideoInput::Type::SOURCE:
		return !!source.GetSource();
	case VideoInput::Type::SCENE:
		return !!scene.GetScene();
	}
	return false;
}

OBSWeakSource VideoInput::GetVideo() const
{
	switch (type) {
	case VideoInput::Type::OBS_MAIN_OUTPUT:
		return nullptr;
	case VideoInput::Type::SOURCE:
		return source.GetSource();
	case VideoInput::Type::SCENE:
		return scene.GetScene();
	}
	return nullptr;
}

OCRParameters::OCRParameters()
{
	Setup();
}

OCRParameters::~OCRParameters()
{
	if (!initDone) {
		return;
	}
	ocr->End();
}

OCRParameters::OCRParameters(const OCRParameters &other)
	: text(other.text),
	  regex(other.regex),
	  color(other.color),
	  colorThreshold(other.colorThreshold),
	  pageSegMode(other.pageSegMode),
	  tesseractBasePath(other.tesseractBasePath),
	  languageCode(other.languageCode),
	  useConfig(other.useConfig),
	  configFile(other.configFile)
{
}

OCRParameters &OCRParameters::operator=(const OCRParameters &other)
{
	text = other.text;
	regex = other.regex;
	color = other.color;
	colorThreshold = other.colorThreshold;
	pageSegMode = other.pageSegMode;
	tesseractBasePath = other.tesseractBasePath;
	languageCode = other.languageCode;
	useConfig = other.useConfig;
	configFile = other.configFile;
	initDone = false;
	return *this;
}

bool OCRParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	text.Save(data, "pattern");
	regex.Save(data);
	tesseractBasePath.Save(data, "tesseractBasePath");
	languageCode.Save(data, "language");
	obs_data_set_bool(data, "useConfig", useConfig);
	obs_data_set_string(data, "configFile", configFile.c_str());
	color.Save(data, "textColor");
	colorThreshold.Save(data, "colorThreshold");
	obs_data_set_int(data, "pageSegMode", static_cast<int>(pageSegMode));
	obs_data_set_int(data, "version", 3);
	obs_data_set_obj(obj, "ocrData", data);
	obs_data_release(data);
	return true;
}

bool OCRParameters::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "ocrData");
	text.Load(data, "pattern");
	regex.Load(data);
	obs_data_set_default_string(data, "language", "eng");
	languageCode.Load(data, "language");
	tesseractBasePath.Load(data, "tesseractBasePath");
	if (!obs_data_has_user_value(data, "version") ||
	    obs_data_get_int(data, "version") < 2) {
		tesseractBasePath = std::string(obs_get_module_data_path(
					    obs_current_module())) +
				    "/res/ocr/";
	} else if (obs_data_get_int(data, "version") == 2) {
#ifdef _WIN32
		// On Windows the plugin directory moved from
		// "Program Files/.../data/obs-plugins/advanced-scene-switcher"
		// to "ProgramData/.../plugins/advanced-scene-switcher/data",
		// which invalidates the previously saved default OCR path.
		const std::string oldPath = tesseractBasePath;
		if (oldPath ==
			    "../../data/obs-plugins/advanced-scene-switcher/res/ocr/" ||
		    oldPath ==
			    "../../data/obs-plugins/advanced-scene-switcher/res/ocr") {
			tesseractBasePath =
				std::string(obs_get_module_data_path(
					obs_current_module())) +
				"/res/ocr/";
		}
#endif
	}
	useConfig = obs_data_get_bool(data, "useConfig");
	configFile = obs_data_get_string(data, "configFile");

	color.Load(data, "textColor");
	if (obs_data_has_user_value(data, "version")) {
		colorThreshold.Load(data, "colorThreshold");
	}
	pageSegMode = static_cast<tesseract::PageSegMode>(
		obs_data_get_int(data, "pageSegMode"));
	obs_data_release(data);

	Setup();
	return true;
}

void OCRParameters::SetPageMode(tesseract::PageSegMode mode)
{
	pageSegMode = mode;
	if (ocr) {
		ocr->SetPageSegMode(mode);
	}
}

tesseract::TessBaseAPI *OCRParameters::GetOCR()
{
	if (!initDone) {
		Setup();
	}
	return initDone ? ocr.get() : nullptr;
}

bool OCRParameters::SetLanguageCode(const std::string &value)
{
	const auto dataPath = QString::fromStdString(tesseractBasePath) + "/" +
			      QString::fromStdString(value) + ".traineddata";
	QFileInfo fileInfo(dataPath);
	if (!fileInfo.exists(dataPath)) {
		return false;
	}
	languageCode = value;
	initDone = false;
	return true;
}

std::string OCRParameters::GetLanguageCode() const
{
	return languageCode;
}

bool OCRParameters::SetTesseractBasePath(const std::string &value)
{
	const auto dataPath = QString::fromStdString(value) + "/" +
			      QString::fromStdString(languageCode) +
			      ".traineddata";
	QFileInfo fileInfo(dataPath);
	if (!fileInfo.exists(dataPath)) {
		return false;
	}
	tesseractBasePath = value;
	initDone = false;
	return true;
}

std::string OCRParameters::GetTesseractBasePath() const
{
	return tesseractBasePath;
}

void OCRParameters::EnableCustomConfig(bool enable)
{
	useConfig = enable;
	initDone = false;
}

void OCRParameters::SetCustomConfigFile(const std::string &filename)
{
	configFile = filename;
	initDone = false;
}

void OCRParameters::Setup()
{
	ocr = std::make_unique<tesseract::TessBaseAPI>();
	if (!ocr) {
		initDone = false;
		return;
	}

	const std::string dataPath = std::string(tesseractBasePath) + "/";
	const std::string modelFile =
		std::string(languageCode) + ".traineddata";
	const auto modelFullPath = QString::fromStdString(dataPath) +
				   QString::fromStdString(modelFile);
	QFileInfo modelFileInfo(modelFullPath);
	if (!modelFileInfo.exists(modelFullPath)) {
		initDone = false;
		blog(LOG_WARNING,
		     "cannot init tesseract! Model path does not exists: %s",
		     modelFileInfo.absoluteFilePath().toStdString().c_str());
		return;
	}

	auto configPath = QString::fromStdString(configFile);
	QFileInfo configFileInfo(configPath);
	bool configFileExists = configFileInfo.exists(configPath);

	bool setupWithConfig = useConfig;
	if (useConfig && !configFileExists) {
		blog(LOG_WARNING,
		     "tesseract config file will be ignored! File does not exists: %s",
		     configFileInfo.absoluteFilePath().toStdString().c_str());
		setupWithConfig = false;
	}

	char *configs[] = {configFile.data()};
	if (ocr->Init(dataPath.c_str(), languageCode.c_str(),
		      tesseract::OEM_DEFAULT,
		      setupWithConfig ? configs : nullptr,
		      setupWithConfig ? 1 : 0, nullptr, nullptr, false) != 0) {
		blog(LOG_WARNING, "tesseract init failed!");
		initDone = false;
		return;
	}

	ocr->SetPageSegMode(pageSegMode);

	initDone = true;
}

bool ColorParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	color.Save(data, "color");
	colorThreshold.Save(data, "colorThreshold");
	matchThreshold.Save(data, "matchThreshold");
	obs_data_set_obj(obj, "colorData", data);
	obs_data_release(data);
	return true;
}

bool ColorParameters::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "colorData");
	color.Load(data, "color");
	colorThreshold.Load(data, "colorThreshold");
	matchThreshold.Load(data, "matchThreshold");
	obs_data_release(data);
	return true;
}

} // namespace advss
