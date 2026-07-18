#pragma once
#if CV_VERSION_MAJOR < 5
#include "object-detector.hpp"
#include <opencv2/objdetect.hpp>

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
#endif
