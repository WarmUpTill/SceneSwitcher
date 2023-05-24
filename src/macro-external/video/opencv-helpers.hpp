#pragma once
#include <QImage>
#undef NO // MacOS macro that can conflict with OpenCV
#include <opencv2/opencv.hpp>

#ifdef OCR_SUPPORT
#include <tesseract/baseapi.h>
#else
namespace tesseract {
enum PageSegMode {
	PSM_OSD_ONLY = 0,
	PSM_AUTO_OSD = 1,
	PSM_AUTO_ONLY = 2,
	PSM_AUTO = 3,
	PSM_SINGLE_COLUMN = 4,
	PSM_SINGLE_BLOCK_VERT_TEXT = 5,
	PSM_SINGLE_BLOCK = 6,
	PSM_SINGLE_LINE = 7,
	PSM_SINGLE_WORD = 8,
	PSM_CIRCLE_WORD = 9,
	PSM_SINGLE_CHAR = 10,
	PSM_SPARSE_TEXT = 11,
	PSM_SPARSE_TEXT_OSD = 12,
	PSM_RAW_LINE = 13,

	PSM_COUNT
};

class TessBaseAPI {
public:
	void SetPageSegMode(PageSegMode) {}
	int Init(const char *, const char *) { return 0; }
	void End() {}
};
}
#endif

namespace advss {

constexpr int minMinNeighbors = 3;
constexpr int maxMinNeighbors = 6;
constexpr double defaultScaleFactor = 1.1;

struct PatternImageData {
	cv::UMat rgbaPattern;
	cv::UMat rgbPattern;
	cv::UMat mask;
};

PatternImageData CreatePatternData(const QImage &pattern);
void MatchPattern(QImage &img, const PatternImageData &patternData,
		  double threshold, cv::UMat &result, bool useAlphaAsMask,
		  cv::TemplateMatchModes matchMode);
void MatchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::UMat &result, bool useAlphaAsMask,
		  cv::TemplateMatchModes matchMode);
std::vector<cv::Rect> MatchObject(QImage &img, cv::CascadeClassifier &cascade,
				  double scaleFactor, int minNeighbors,
				  const cv::Size &minSize,
				  const cv::Size &maxSize);
uchar GetAvgBrightness(QImage &img);
cv::UMat PreprocessForOCR(const QImage &image, const QColor &color);
std::string RunOCR(tesseract::TessBaseAPI *, const QImage &, const QColor &);
bool ContainsPixelsInColorRange(const QImage &image, const QColor &color,
				double colorDeviationThreshold,
				double totalPixelMatchThreshold);
cv::UMat QImageToMat(const QImage &img);
QImage MatToQImage(const cv::Mat &mat);
void SetupOpenCL();

} // namespace advss
