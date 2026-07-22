#pragma once
#include "object-detector.hpp"
#if CV_VERSION_MAJOR < 5
#include <opencv2/objdetect.hpp>
#else
#include <opencv2/xobjdetect.hpp>
#endif

namespace advss {

class CascadeClassifierDetector : public ObjectDetector {
public:
	bool Load(const std::string &modelPath) override;
	bool IsLoaded() const override;
	std::vector<cv::Rect> Detect(QImage &img) override;

	double scaleFactor = 1.1;
	int minNeighbors = 3;
	cv::Size minSize{0, 0};
	cv::Size maxSize{0, 0};

private:
	cv::CascadeClassifier _cascade;
};

} // namespace advss
