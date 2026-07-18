#include "cascade-classifier-detector.hpp"
#if CV_VERSION_MAJOR < 5

#include "opencv-helpers.hpp"

#include <log-helper.hpp>

namespace advss {

bool CascadeClassifierDetector::Load(const std::string &modelPath)
{
	try {
		if (!_cascade.load(modelPath)) {
			blog(LOG_WARNING, "failed to load cascade model \"%s\"",
			     modelPath.c_str());
			return false;
		}
	} catch (...) {
		blog(LOG_WARNING, "failed to load cascade model \"%s\"",
		     modelPath.c_str());
		return false;
	}
	return !_cascade.empty();
}

bool CascadeClassifierDetector::IsLoaded() const
{
	return !_cascade.empty();
}

std::vector<cv::Rect> CascadeClassifierDetector::Detect(QImage &img)
{
	if (img.isNull() || _cascade.empty()) {
		return {};
	}

	auto image = QImageToMat(img);
	cv::Mat frameGray;
	cv::cvtColor(image, frameGray, cv::COLOR_RGBA2GRAY);
	cv::equalizeHist(frameGray, frameGray);

	std::vector<cv::Rect> objects;
	try {
		_cascade.detectMultiScale(frameGray, objects, scaleFactor,
					  minNeighbors, 0, minSize, maxSize);
	} catch (const std::exception &e) {
		vblog(LOG_INFO, "detectMultiScale failed: %s", e.what());
	}
	return objects;
}

} // namespace advss
#endif
