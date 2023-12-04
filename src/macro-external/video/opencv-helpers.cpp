#include "opencv-helpers.hpp"

#include <log-helper.hpp>

namespace advss {

PatternImageData CreatePatternData(const QImage &pattern)
{
	PatternImageData data{};
	if (pattern.isNull()) {
		return data;
	}

	data.rgbaPattern = QImageToMat(pattern);
	std::vector<cv::Mat1b> rgbaChannelsPattern;
	cv::split(data.rgbaPattern, rgbaChannelsPattern);
	std::vector<cv::Mat1b> rgbChanlesPattern(
		rgbaChannelsPattern.begin(), rgbaChannelsPattern.begin() + 3);
	cv::merge(rgbChanlesPattern, data.rgbPattern);
	cv::threshold(rgbaChannelsPattern[3], data.mask, 0, 255,
		      cv::THRESH_BINARY);
	return data;
}

static void invertPatternMatchResult(cv::Mat &mat)
{
	for (int r = 0; r < mat.rows; r++) {
		for (int c = 0; c < mat.cols; c++) {
			mat.at<float>(r, c) = 1.0 - mat.at<float>(r, c);
		}
	}
}

void MatchPattern(QImage &img, const PatternImageData &patternData,
		  double threshold, cv::Mat &result, bool useAlphaAsMask,
		  cv::TemplateMatchModes matchMode)
{
	if (img.isNull() || patternData.rgbaPattern.empty()) {
		return;
	}
	if (img.height() < patternData.rgbaPattern.rows ||
	    img.width() < patternData.rgbaPattern.cols) {
		return;
	}

	auto input = QImageToMat(img);

	if (useAlphaAsMask) {
		// Remove alpha channel of input image as the alpha channel
		// information is used as a stencil for the pattern instead and
		// thus should not be used while matching the pattern as well
		//
		// Input format is Format_RGBA8888 so discard the 4th channel
		std::vector<cv::Mat1b> inputChannels;
		cv::split(input, inputChannels);
		std::vector<cv::Mat1b> rgbChanlesImage(
			inputChannels.begin(), inputChannels.begin() + 3);
		cv::Mat3b rgbInput;
		cv::merge(rgbChanlesImage, rgbInput);

		cv::matchTemplate(rgbInput, patternData.rgbPattern, result,
				  matchMode, patternData.mask);
	} else {
		cv::matchTemplate(input, patternData.rgbaPattern, result,
				  matchMode);
	}

	// A perfect match is represented as "0" for TM_SQDIFF_NORMED
	//
	// For TM_CCOEFF_NORMED and TM_CCORR_NORMED a perfect match is
	// represented as "1"
	if (matchMode == cv::TM_SQDIFF_NORMED) {
		invertPatternMatchResult(result);
	}
	cv::threshold(result, result, threshold, 0.0, cv::THRESH_TOZERO);
}

void MatchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::Mat &result, bool useAlphaAsMask,
		  cv::TemplateMatchModes matchColor)
{
	auto data = CreatePatternData(pattern);
	MatchPattern(img, data, threshold, result, useAlphaAsMask, matchColor);
}

std::vector<cv::Rect> MatchObject(QImage &img, cv::CascadeClassifier &cascade,
				  double scaleFactor, int minNeighbors,
				  const cv::Size &minSize,
				  const cv::Size &maxSize)
{
	if (img.isNull() || cascade.empty()) {
		return {};
	}

	auto image = QImageToMat(img);
	cv::Mat frameGray;
	cv::cvtColor(image, frameGray, cv::COLOR_RGBA2GRAY);
	cv::equalizeHist(frameGray, frameGray);
	std::vector<cv::Rect> objects;
	try {
		cascade.detectMultiScale(frameGray, objects, scaleFactor,
					 minNeighbors, 0, minSize, maxSize);
	} catch (const std::exception &e) {
		vblog(LOG_INFO, "detectMultiScale failed: %s", e.what());
	}
	return objects;
}

uchar GetAvgBrightness(QImage &img)
{
	if (img.isNull()) {
		return 0;
	}

	auto image = QImageToMat(img);
	cv::Mat hsvImage, rgbImage;
	cv::cvtColor(image, rgbImage, cv::COLOR_RGBA2RGB);
	cv::cvtColor(rgbImage, hsvImage, cv::COLOR_RGB2HSV);
	long long brightnessSum = 0;
	for (int i = 0; i < hsvImage.rows; ++i) {
		for (int j = 0; j < hsvImage.cols; ++j) {
			brightnessSum += hsvImage.at<cv::Vec3b>(i, j)[2];
		}
	}
	return brightnessSum / (hsvImage.rows * hsvImage.cols);
}

static bool colorIsSimilar(const QColor &color1, const QColor &color2,
			   int maxDiff)
{
	const int diffRed = std::abs(color1.red() - color2.red());
	const int diffGreen = std::abs(color1.green() - color2.green());
	const int diffBlue = std::abs(color1.blue() - color2.blue());

	return diffRed <= maxDiff && diffGreen <= maxDiff &&
	       diffBlue <= maxDiff;
}

cv::Mat PreprocessForOCR(const QImage &image, const QColor &textColor,
			 double colorDiff)
{
	auto mat = QImageToMat(image);

	// Tesseract works best when matching black text on a white background,
	// so everything that matches the text color will be displayed black
	// while the rest of the image should be white.
	const int diff = colorDiff * 255;
	for (int y = 0; y < image.height(); y++) {
		for (int x = 0; x < image.width(); x++) {
			if (colorIsSimilar(image.pixelColor(x, y), textColor,
					   diff)) {
				mat.at<cv::Vec4b>(y, x) = {0, 0, 0, 255};
			} else {
				mat.at<cv::Vec4b>(y, x) = {255, 255, 255, 255};
			}
		}
	}

	// Scale image up if selected area is very small.
	// Results will probably still be unsatisfying.
	if (mat.rows <= 300 || mat.cols <= 300) {
		double scale = 0.;
		if (mat.rows < mat.cols) {
			scale = 300. / mat.rows;
		} else {
			scale = 300. / mat.cols;
		}
		cv::resize(mat, mat,
			   cv::Size(mat.cols * scale, mat.rows * scale),
			   cv::INTER_CUBIC);
	}

	cv::Mat result;
	mat.copyTo(result);
	return result;
}

std::string RunOCR(tesseract::TessBaseAPI *ocr, const QImage &image,
		   const QColor &color, double colorDiff)
{
	(void)ocr;
	(void)color;
	(void)colorDiff;
	if (image.isNull()) {
		return "";
	}

#ifdef OCR_SUPPORT
	auto mat = PreprocessForOCR(image, color, colorDiff);
	cv::Mat gray;
	cv::cvtColor(mat, gray, cv::COLOR_RGBA2GRAY);
	ocr->SetImage(gray.data, gray.cols, gray.rows, 1, gray.step);
	ocr->Recognize(0);
	std::unique_ptr<char[]> detectedText(ocr->GetUTF8Text());

	if (!detectedText) {
		return "";
	}
	return detectedText.get();

#else
	return "";
#endif
}

bool ContainsPixelsInColorRange(const QImage &image, const QColor &color,
				double colorDeviationThreshold,
				double totalPixelMatchThreshold)
{
	int totalPixels = image.width() * image.height();
	int matchingPixels = 0;
	int maxColorDiff = static_cast<int>(colorDeviationThreshold * 255.0);

	for (int y = 0; y < image.height(); y++) {
		for (int x = 0; x < image.width(); x++) {
			if (colorIsSimilar(image.pixelColor(x, y), color,
					   maxColorDiff)) {
				matchingPixels++;
			}
		}
	}

	double matchPercentage =
		static_cast<double>(matchingPixels) / totalPixels;
	return matchPercentage >= totalPixelMatchThreshold;
}

QColor GetAverageColor(const QImage &img)
{
	if (img.isNull()) {
		return QColor();
	}

	auto image = QImageToMat(img);
	cv::Scalar meanColor = cv::mean(image);
	int averageBlue = cvRound(meanColor[0]);
	int averageGreen = cvRound(meanColor[1]);
	int averageRed = cvRound(meanColor[2]);

	return QColor(averageRed, averageGreen, averageBlue);
}

QColor GetDominantColor(const QImage &img, int k)
{
	if (img.isNull()) {
		return QColor();
	}

	auto image = QImageToMat(img);
	cv::Mat reshapedImage = image.reshape(1, image.rows * image.cols);
	reshapedImage.convertTo(reshapedImage, CV_32F);

	cv::mean(reshapedImage);

	// Apply k-means clustering to group similar colors
	cv::TermCriteria criteria(
		cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 0.2);
	cv::Mat labels, centers;
	cv::kmeans(reshapedImage, k, labels, criteria, 1,
		   cv::KMEANS_RANDOM_CENTERS, centers);

	// Find the dominant color
	// Center of the cluster with the largest number of pixels
	cv::Mat counts = cv::Mat::zeros(1, k, CV_32SC1);
	for (int i = 0; i < labels.rows; i++) {
		counts.at<int>(0, labels.at<int>(i))++;
	}

	cv::Point max_loc;
	cv::minMaxLoc(counts, nullptr, nullptr, nullptr, &max_loc);
	try {
		cv::Scalar dominantColor = centers.at<cv::Scalar>(max_loc.y);
		const int blue = cv::saturate_cast<int>(dominantColor.val[0]);
		const int green = cv::saturate_cast<int>(dominantColor.val[1]);
		const int red = cv::saturate_cast<int>(dominantColor.val[2]);
		const int alpha = cv::saturate_cast<int>(dominantColor.val[3]);
		return QColor(red, green, blue, alpha);
	} catch (...) {
	}
	return QColor();
}

// Assumption is that QImage uses Format_RGBA8888.
// Conversion from: https://github.com/dbzhang800/QtOpenCV
cv::Mat QImageToMat(const QImage &img)
{
	if (img.isNull()) {
		return cv::Mat();
	}
	return cv::Mat(img.height(), img.width(), CV_8UC(img.depth() / 8),
		       (uchar *)img.bits(), img.bytesPerLine());
}

QImage MatToQImage(const cv::Mat &mat)
{
	if (mat.empty()) {
		return QImage();
	}
	return QImage(mat.data, mat.cols, mat.rows,
		      QImage::Format::Format_RGBA8888);
}

} // namespace advss
