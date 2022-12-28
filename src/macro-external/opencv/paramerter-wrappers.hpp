#pragma once
#include "opencv-helpers.hpp"
#include "area-selection.hpp"

#include <source-selection.hpp>
#include <scene-selection.hpp>
#include <obs.hpp>
#include <obs-module.h>

enum class VideoCondition {
	MATCH,
	DIFFER,
	HAS_NOT_CHANGED,
	HAS_CHANGED,
	NO_IMAGE,
	PATTERN,
	OBJECT,
	BRIGHTNESS,
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
	double threshold = 0.8;
};

class ObjDetectParamerts {
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

class AreaParamters {
public:
	bool Save(obs_data_t *obj) const;
	bool Load(obs_data_t *obj);

	bool enable = false;
	advss::Area area{0, 0, 0, 0};
};
