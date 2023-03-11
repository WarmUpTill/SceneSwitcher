#pragma once
#include "opencv-helpers.hpp"
#include "area-selection.hpp"

#include <source-selection.hpp>
#include <scene-selection.hpp>
#include <regex-config.hpp>
#include <variable-string.hpp>
#include <obs.hpp>
#include <obs-module.h>

#include <QMetaType>

#ifdef OCR_SUPPORT
#include <tesseract/baseapi.h>
#endif

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT,
	BRIGHTNESS,
	OCR,
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
	double threshold = 0.8;
};

class ObjDetectParameters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	std::string modelPath =
		obs_get_module_data_path(obs_current_module()) +
		std::string(
			"/res/cascadeClassifiers/haarcascade_frontalface_alt.xml");
	cv::CascadeClassifier cascade;
	double scaleFactor = defaultScaleFactor;
	int minNeighbors = minMinNeighbors;
	advss::Size minSize{0, 0};
	advss::Size maxSize{0, 0};
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
	tesseract::PageSegMode GetPageMode() const { return pageSegMode; }
	tesseract::TessBaseAPI *GetOCR() const { return ocr.get(); }

	VariableString text = obs_module_text("AdvSceneSwitcher.enterText");
	RegexConfig regex = RegexConfig::PartialMatchRegexConfig();
	QColor color = Qt::black;

private:
	void Setup();

	tesseract::PageSegMode pageSegMode = tesseract::PSM_SINGLE_BLOCK;
	std::unique_ptr<tesseract::TessBaseAPI> ocr;
	bool initDone = false;
};

Q_DECLARE_METATYPE(OCRParameters)

class AreaParamters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	bool enable = false;
	advss::Area area{0, 0, 0, 0};
};
