#pragma once
#include "object-detector.hpp"
#include "opencv-helpers.hpp"
#include "obs-module-helper.hpp"
#include "area-selection.hpp"

#include <source-selection.hpp>
#include <scene-selection.hpp>
#include <regex-config.hpp>
#include <variable-color.hpp>
#include <variable-string.hpp>
#include <variable-number.hpp>
#include <obs.hpp>
#include <obs-module.h>

#include <QMetaType>

#ifdef OCR_SUPPORT
#include <tesseract/baseapi.h>
#endif

namespace advss {

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT_CASCADE,
	BRIGHTNESS,
	OCR,
	COLOR,
	OBJECT_DNN,
};

class VideoInput {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);
	std::string ToString(bool resolve = false) const;
	bool ValidSelection() const;
	OBSWeakSource GetVideo() const;

	enum class Type {
		OBS_MAIN_OUTPUT,
		SOURCE,
		SCENE,
	};

	Type type = Type::OBS_MAIN_OUTPUT;
	SourceSelection source;
	SceneSelection scene;
};

class PatternMatchParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	QImage image;
	bool useForChangedCheck = false;
	bool useAlphaAsMask = false;
	cv::TemplateMatchModes matchMode = cv::TM_CCORR_NORMED;
	NumberVariable<double> threshold = 0.999;
};

class CascadeClassifierParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	CascadeClassifierParameters() = default;
	CascadeClassifierParameters(const CascadeClassifierParameters &other)
		: scaleFactor(other.scaleFactor),
		  minNeighbors(other.minNeighbors),
		  minSize(other.minSize),
		  maxSize(other.maxSize),
		  _modelPath(other._modelPath)
	{
	}
	CascadeClassifierParameters &
	operator=(const CascadeClassifierParameters &other)
	{
		if (this != &other) {
			scaleFactor = other.scaleFactor;
			minNeighbors = other.minNeighbors;
			minSize = other.minSize;
			maxSize = other.maxSize;
			_modelPath = other._modelPath;
			_detector.reset();
		}
		return *this;
	}

	bool SetModelPath(const std::string &path);
	const std::string &GetModelPath() const { return _modelPath; }
	ObjectDetector *GetDetector();

	NumberVariable<double> scaleFactor = defaultScaleFactor;
	int minNeighbors = minMinNeighbors;
	Size minSize{0, 0};
	Size maxSize{0, 0};

private:
	bool LoadModelData();

	std::unique_ptr<ObjectDetector> _detector;
	std::string _modelPath =
		obs_get_module_data_path(obs_current_module()) +
		std::string(
			"/res/cascadeClassifiers/haarcascade_frontalface_alt.xml");
};

class DnnDetectParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	DnnDetectParameters() = default;
	DnnDetectParameters(const DnnDetectParameters &other)
		: confidenceThreshold(other.confidenceThreshold),
		  nmsThreshold(other.nmsThreshold),
		  inputSize(other.inputSize),
		  scaleFactor(other.scaleFactor),
		  meanR(other.meanR),
		  meanG(other.meanG),
		  meanB(other.meanB),
		  swapRB(other.swapRB),
		  _modelPath(other._modelPath),
		  _configPath(other._configPath)
	{
	}
	DnnDetectParameters &operator=(const DnnDetectParameters &other)
	{
		if (this != &other) {
			confidenceThreshold = other.confidenceThreshold;
			nmsThreshold = other.nmsThreshold;
			inputSize = other.inputSize;
			scaleFactor = other.scaleFactor;
			meanR = other.meanR;
			meanG = other.meanG;
			meanB = other.meanB;
			swapRB = other.swapRB;
			_modelPath = other._modelPath;
			_configPath = other._configPath;
			_detector.reset();
		}
		return *this;
	}

	bool SetModelPath(const std::string &path);
	const std::string &GetModelPath() const { return _modelPath; }
	bool SetConfigPath(const std::string &path);
	const std::string &GetConfigPath() const { return _configPath; }
	ObjectDetector *GetDetector();

	NumberVariable<double> confidenceThreshold = 0.5;
	NumberVariable<double> nmsThreshold = 0.4;
	Size inputSize = {300, 300};
	double scaleFactor = 1.0 / 127.5;
	double meanR = 127.5;
	double meanG = 127.5;
	double meanB = 127.5;
	bool swapRB = true;

private:
	bool LoadModelData();

	std::unique_ptr<ObjectDetector> _detector;
	std::string _modelPath;
	std::string _configPath;
};

class OCRParameters {
public:
	OCRParameters();
	~OCRParameters();
	OCRParameters(const OCRParameters &other);
	OCRParameters &operator=(const OCRParameters &);

	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	bool Initialized() const { return initDone; }
	void SetPageMode(tesseract::PageSegMode);
	bool SetLanguageCode(const std::string &);
	std::string GetLanguageCode() const;
	bool SetTesseractBasePath(const std::string &);
	std::string GetTesseractBasePath() const;
	void EnableCustomConfig(bool enable);
	bool CustomConfigIsEnabled() const { return useConfig; }
	void SetCustomConfigFile(const std::string &);
	std::string GetCustomConfigFile() const { return configFile; }
	tesseract::PageSegMode GetPageMode() const { return pageSegMode; }
	tesseract::TessBaseAPI *GetOCR();

	StringVariable text = obs_module_text("AdvSceneSwitcher.enterText");
	RegexConfig regex = RegexConfig::PartialMatchRegexConfig();
	ColorVariable color;
	DoubleVariable colorThreshold = 0.3;

private:
	void Setup();

	tesseract::PageSegMode pageSegMode = tesseract::PSM_SINGLE_BLOCK;
	StringVariable tesseractBasePath =
		obs_get_module_data_path(obs_current_module()) +
		std::string("/res/ocr");
	StringVariable languageCode = "eng";
	bool useConfig = false;
	std::string configFile = "config.txt";
	std::unique_ptr<tesseract::TessBaseAPI> ocr;
	bool initDone = false;
};

class ColorParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	ColorVariable color;
	DoubleVariable colorThreshold = 0.1;
	DoubleVariable matchThreshold = 0.8;
};

class AreaParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	bool enable = false;
	advss::Area area{0, 0, 0, 0};
};

} // namespace advss

Q_DECLARE_METATYPE(advss::OCRParameters)
