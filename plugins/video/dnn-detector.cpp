#include "dnn-detector.hpp"

#ifdef HAVE_OPENCV_DNN

#include "opencv-helpers.hpp"

#include <log-helper.hpp>

namespace advss {

bool DnnDetector::Load(const std::string &modelPath)
{
	try {
		_net = cv::dnn::readNet(modelPath);
	} catch (const std::exception &e) {
		blog(LOG_WARNING, "failed to load DNN model \"%s\": %s",
		     modelPath.c_str(), e.what());
		return false;
	}
	return !_net.empty();
}

bool DnnDetector::IsLoaded() const
{
	return !_net.empty();
}

std::vector<cv::Rect> DnnDetector::Detect(QImage &img)
{
	if (img.isNull() || _net.empty()) {
		return {};
	}

	auto mat = QImageToMat(img);
	cv::Mat bgr;
	cv::cvtColor(mat, bgr, cv::COLOR_RGBA2BGR);

	cv::Mat blob = cv::dnn::blobFromImage(bgr, scaleFactor,
					      cv::Size(inputWidth, inputHeight),
					      cv::Scalar(meanR, meanG, meanB),
					      swapRB, false);
	_net.setInput(blob);

	cv::Mat output;
	try {
		output = _net.forward();
	} catch (const std::exception &e) {
		vblog(LOG_INFO, "DNN forward pass failed: %s", e.what());
		return {};
	}

	// output shape: [1, 1, N, 7]
	if (output.dims < 4) {
		vblog(LOG_INFO,
		      "unexpected DNN output dimensions: %d (expected 4)",
		      output.dims);
		return {};
	}

	const int imgWidth = img.width();
	const int imgHeight = img.height();
	const int numDetections = output.size[2];
	const cv::Mat detMat(numDetections, output.size[3], CV_32F,
			     output.ptr<float>());

	std::vector<cv::Rect> boxes;
	std::vector<float> confidences;

	for (int i = 0; i < numDetections; i++) {
		const float confidence = detMat.at<float>(i, 2);
		if (confidence < static_cast<float>(confidenceThreshold)) {
			continue;
		}
		const int left =
			static_cast<int>(detMat.at<float>(i, 3) * imgWidth);
		const int top =
			static_cast<int>(detMat.at<float>(i, 4) * imgHeight);
		const int right =
			static_cast<int>(detMat.at<float>(i, 5) * imgWidth);
		const int bottom =
			static_cast<int>(detMat.at<float>(i, 6) * imgHeight);
		boxes.push_back(
			cv::Rect(left, top, right - left, bottom - top));
		confidences.push_back(confidence);
	}

	std::vector<int> indices;
	cv::dnn::NMSBoxes(boxes, confidences,
			  static_cast<float>(confidenceThreshold),
			  static_cast<float>(nmsThreshold), indices);

	std::vector<cv::Rect> results;
	results.reserve(indices.size());
	for (int idx : indices) {
		results.push_back(boxes[idx]);
	}
	return results;
}

} // namespace advss

#endif // HAVE_OPENCV_DNN
