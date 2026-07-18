#include "cascade-classifier-detector.hpp"

#ifdef ADVSS_CASCADE_SUPPORT

#include "opencv-helpers.hpp"
#include "log-helper.hpp"

#if CV_VERSION_MAJOR < 5
#include <opencv2/objdetect.hpp>
#else
#include <opencv2/xobjdetect.hpp>
#endif

namespace advss {

struct CascadeClassifierDetector::Impl {
	cv::CascadeClassifier cascade;
};

CascadeClassifierDetector::CascadeClassifierDetector()
	: _impl(std::make_unique<Impl>())
{
}

CascadeClassifierDetector::~CascadeClassifierDetector() = default;

bool CascadeClassifierDetector::IsSupported()
{
	return true;
}

bool CascadeClassifierDetector::Load(const std::string &modelPath)
{
	try {
		if (!_impl->cascade.load(modelPath)) {
			blog(LOG_WARNING, "failed to load cascade model \"%s\"",
			     modelPath.c_str());
			return false;
		}
	} catch (...) {
		blog(LOG_WARNING, "failed to load cascade model \"%s\"",
		     modelPath.c_str());
		return false;
	}
	return !_impl->cascade.empty();
}

bool CascadeClassifierDetector::IsLoaded() const
{
	return !_impl->cascade.empty();
}

std::vector<cv::Rect> CascadeClassifierDetector::Detect(QImage &img)
{
	if (img.isNull() || _impl->cascade.empty()) {
		return {};
	}

	auto image = QImageToMat(img);
	cv::Mat frameGray;
	cv::cvtColor(image, frameGray, cv::COLOR_RGBA2GRAY);
	cv::equalizeHist(frameGray, frameGray);

	std::vector<cv::Rect> objects;
	try {
		_impl->cascade.detectMultiScale(frameGray, objects, scaleFactor,
						minNeighbors, 0, minSize,
						maxSize);
	} catch (const std::exception &e) {
		vblog(LOG_INFO, "detectMultiScale failed: %s", e.what());
	}
	return objects;
}

} // namespace advss

#else // ADVSS_CASCADE_SUPPORT

namespace advss {

struct CascadeClassifierDetector::Impl {};

CascadeClassifierDetector::CascadeClassifierDetector()
	: _impl(std::make_unique<Impl>())
{
}

CascadeClassifierDetector::~CascadeClassifierDetector() = default;

bool CascadeClassifierDetector::IsSupported()
{
	return false;
}
bool CascadeClassifierDetector::Load(const std::string &)
{
	return false;
}
bool CascadeClassifierDetector::IsLoaded() const
{
	return false;
}
std::vector<cv::Rect> CascadeClassifierDetector::Detect(QImage &)
{
	return {};
}

} // namespace advss

#endif // ADVSS_CASCADE_SUPPORT
