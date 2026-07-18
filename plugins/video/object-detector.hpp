#pragma once
#include <opencv2/opencv.hpp>
#include <QImage>
#include <string>
#include <vector>

namespace advss {

struct ObjectDetector {
	virtual bool Load(const std::string &modelPath) = 0;
	virtual bool IsLoaded() const = 0;
	virtual std::vector<cv::Rect> Detect(QImage &img) = 0;
	virtual ~ObjectDetector() = default;
};

} // namespace advss
