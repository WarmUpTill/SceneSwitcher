#include "parameter-wrappers.hpp"
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

bool ObjDetectParameters::Save(obs_data_t *obj) const
{
	auto data = obs_data_create();
	obs_data_set_string(data, "modelPath", modelPath.c_str());
	scaleFactor.Save(data, "scaleFactor");
	obs_data_set_int(data, "minNeighbors", minNeighbors);
	minSize.Save(data, "minSize");
	maxSize.Save(data, "maxSize");
	obs_data_set_obj(obj, "objectMatchData", data);
	obs_data_set_int(data, "version", 1);
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

bool ObjDetectParameters::Load(obs_data_t *obj)
{
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(obj, "patternMatchData")) {
		modelPath = obs_data_get_string(obj, "modelDataPath");
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
	modelPath = obs_data_get_string(data, "modelPath");
	scaleFactor.Load(data, "scaleFactor");
	// TODO: Remove this fallback in a future version
	if (!obs_data_has_user_value(data, "version")) {
		scaleFactor = obs_data_get_double(data, "scaleFactor");
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

static void SaveColor(obs_data_t *obj, const char *name, const QColor &color)
{
	auto data = obs_data_create();
	obs_data_set_int(data, "red", color.red());
	obs_data_set_int(data, "green", color.green());
	obs_data_set_int(data, "blue", color.blue());
	obs_data_set_obj(obj, name, data);
	obs_data_release(data);
}

static QColor LoadColor(obs_data_t *obj, const char *name)
{
	QColor color = Qt::black;
	auto data = obs_data_get_obj(obj, name);
	color.setRed(obs_data_get_int(data, "red"));
	color.setGreen(obs_data_get_int(data, "green"));
	color.setBlue(obs_data_get_int(data, "blue"));
	obs_data_release(data);
	return color;
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
	if (!initDone) {
		Setup();
	}
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
	if (!initDone) {
		Setup();
	}
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
	SaveColor(data, "textColor", color);
	colorThreshold.Save(data, "colorThreshold");
	obs_data_set_int(data, "pageSegMode", static_cast<int>(pageSegMode));
	obs_data_set_int(data, "version", 2);
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
	}
	useConfig = obs_data_get_bool(data, "useConfig");
	configFile = obs_data_get_string(data, "configFile");

	color = LoadColor(data, "textColor");
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

bool OCRParameters::SetLanguageCode(const std::string &value)
{
	const auto dataPath = QString::fromStdString(tesseractBasePath) + "/" +
			      QString::fromStdString(value) + ".traineddata";
	QFileInfo fileInfo(dataPath);
	if (!fileInfo.exists(dataPath)) {
		return false;
	}
	languageCode = value;
	Setup();
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
	Setup();
	return true;
}

std::string OCRParameters::GetTesseractBasePath() const
{
	return tesseractBasePath;
}

void OCRParameters::EnableCustomConfig(bool enable)
{
	useConfig = enable;
	Setup();
}

void OCRParameters::SetCustomConfigFile(const std::string &filename)
{
	configFile = filename;
	Setup();
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
	SaveColor(data, "color", color);
	colorThreshold.Save(data, "colorThreshold");
	matchThreshold.Save(data, "matchThreshold");
	obs_data_set_obj(obj, "colorData", data);
	obs_data_release(data);
	return true;
}

bool ColorParameters::Load(obs_data_t *obj)
{
	auto data = obs_data_get_obj(obj, "colorData");
	color = LoadColor(data, "color");
	colorThreshold.Load(data, "colorThreshold");
	matchThreshold.Load(data, "matchThreshold");
	obs_data_release(data);
	return true;
}

} // namespace advss
