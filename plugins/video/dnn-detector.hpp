#pragma once
#include "object-detector.hpp"

#include <opencv2/opencv_modules.hpp>

#ifdef HAVE_OPENCV_DNN

#include <opencv2/dnn.hpp>

namespace advss {

// Object detector backed by cv::dnn, targeting models with SSD-style output.
//
// Expected output shape: [1, 1, N, 7], where each row is:
//   [batch_id, class_id, confidence, left, top, right, bottom]
// Coordinates are normalized to [0, 1] relative to the input image size.
//
// Compatible with models like MobileNet-SSD (ONNX or Caffe format).
//
// Preprocessing parameters (inputWidth, inputHeight, scaleFactor, mean*,
// swapRB) must be matched to the specific model being used.
// Common examples:
//   MobileNet-SSD (Caffe): 300x300, scale=1/127.5, mean=127.5, swapRB=false
//   YOLO (ONNX):           416x416, scale=1/255,   mean=0,     swapRB=true
class DnnDetector : public ObjectDetector {
public:
	bool Load(const std::string &modelPath) override;
	bool IsLoaded() const override;
	std::vector<cv::Rect> Detect(QImage &img) override;

	std::string configPath;

	double confidenceThreshold = 0.5;
	double nmsThreshold = 0.4;
	int inputWidth = 300;
	int inputHeight = 300;
	double scaleFactor = 1.0 / 127.5;
	double meanR = 127.5;
	double meanG = 127.5;
	double meanB = 127.5;
	bool swapRB = true;

private:
	cv::dnn::Net _net;
};

} // namespace advss

#endif // HAVE_OPENCV_DNN
