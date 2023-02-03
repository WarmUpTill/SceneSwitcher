#pragma once
#include <QImage>
#undef NO // MacOS macro that can conflict with OpenCV
#include <opencv2/opencv.hpp>

constexpr int minMinNeighbors = 3;
constexpr int maxMinNeighbors = 6;
constexpr double defaultScaleFactor = 1.1;

struct PatternImageData {
	cv::Mat4b rgbaPattern;
	cv::Mat3b rgbPattern;
	cv::Mat1b mask;
};

PatternImageData createPatternData(QImage &pattern);
void matchPattern(QImage &img, const PatternImageData &patternData,
		  double threshold, cv::Mat &result,
		  bool useAlphaAsMask = true);
void matchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::Mat &result, bool useAlphaAsMask);
std::vector<cv::Rect> matchObject(QImage &img, cv::CascadeClassifier &cascade,
				  double scaleFactor, int minNeighbors,
				  cv::Size minSize, cv::Size maxSize);
uchar getAvgBrightness(QImage &img);
cv::Mat QImageToMat(const QImage &img);
QImage MatToQImage(const cv::Mat &mat);
