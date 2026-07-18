#pragma once
#include "object-detector.hpp"
#include <memory>

namespace advss {

class CascadeClassifierDetector : public ObjectDetector {
public:
	CascadeClassifierDetector();
	~CascadeClassifierDetector() override;

	static bool IsSupported();
	bool Load(const std::string &modelPath) override;
	bool IsLoaded() const override;
	std::vector<cv::Rect> Detect(QImage &img) override;

	double scaleFactor = 1.1;
	int minNeighbors = 3;
	cv::Size minSize{0, 0};
	cv::Size maxSize{0, 0};

private:
	struct Impl;
	std::unique_ptr<Impl> _impl;
};

} // namespace advss
