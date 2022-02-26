#include "video-match-dialog.hpp"
#include "macro-condition-video.hpp"
#include "opencv-helpers.hpp"
#include "utility.hpp"

ShowMatchDialog::ShowMatchDialog(QWidget *parent,
				 MacroConditionVideo *conditionData)
	: QDialog(parent),
	  _conditionData(conditionData),
	  _imageLabel(new QLabel),
	  _scrollArea(new QScrollArea)
{
	setWindowTitle("Advanced Scene Switcher");
	_statusLabel = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.showMatch.loading"));

	_scrollArea->setBackgroundRole(QPalette::Dark);
	_scrollArea->setWidget(_imageLabel);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_statusLabel);
	layout->addWidget(_scrollArea);
	setLayout(layout);
}

ShowMatchDialog::~ShowMatchDialog()
{
	_stop = true;
	if (_thread.joinable()) {
		_thread.join();
	}
}

void ShowMatchDialog::ShowMatch()
{
	if (_thread.joinable()) {
		return;
	}
	if (!_conditionData) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		return;
	}
	_thread = std::thread(&ShowMatchDialog::CheckForMatchLoop, this);
}

void ShowMatchDialog::RedrawImage(QImage img)
{
	_imageLabel->setPixmap(QPixmap::fromImage(img));
	_imageLabel->adjustSize();
}

void ShowMatchDialog::CheckForMatchLoop()
{
	while (!_stop) {
		auto source = obs_weak_source_get_source(
			_conditionData->_videoSource);
		ScreenshotHelper screenshot(source);
		obs_source_release(source);
		std::this_thread::sleep_for(std::chrono::seconds(1));
		if (!screenshot.done) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotFail"));
			_imageLabel->setPixmap(QPixmap());
			continue;
		}
		if (screenshot.image.width() == 0 ||
		    screenshot.image.height() == 0) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.screenshotEmpty"));
			_imageLabel->setPixmap(QPixmap());
			continue;
		}
		auto image = MarkMatch(screenshot.image);
		if (_stop) {
			return;
		}
		QMetaObject::invokeMethod(this, "RedrawImage",
					  Qt::BlockingQueuedConnection,
					  Q_ARG(QImage, image));
	}
}

QImage markPatterns(cv::Mat &matchResult, QImage &image, QImage &pattern)
{
	auto matchImg = QImageToMat(image);
	for (int row = 0; row < matchResult.rows - 1; row++) {
		for (int col = 0; col < matchResult.cols - 1; col++) {
			if (matchResult.at<float>(row, col) != 0.0) {
				rectangle(matchImg, {col, row},
					  cv::Point(col + pattern.width(),
						    row + pattern.height()),
					  cv::Scalar(255, 0, 0, 255), 2, 8, 0);
			}
		}
	}
	return MatToQImage(matchImg);
}

QImage markObjects(QImage &image, std::vector<cv::Rect> &objects)
{
	auto frame = QImageToMat(image);
	for (size_t i = 0; i < objects.size(); i++) {
		rectangle(frame, cv::Point(objects[i].x, objects[i].y),
			  cv::Point(objects[i].x + objects[i].width,
				    objects[i].y + objects[i].height),
			  cv::Scalar(255, 0, 0, 255), 2, 8, 0);
	}
	return MatToQImage(frame);
}

QImage ShowMatchDialog::MarkMatch(QImage &screenshot)
{
	QImage resultIamge;
	if (_conditionData->_condition == VideoCondition::PATTERN) {
		cv::Mat result;
		QImage pattern = _conditionData->GetMatchImage();
		matchPattern(screenshot, pattern,
			     _conditionData->_patternThreshold, result,
			     _conditionData->_useAlphaAsMask);
		if (countNonZero(result) == 0) {
			resultIamge = screenshot;
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchSuccess"));
			resultIamge = markPatterns(result, screenshot, pattern);
		}
	} else if (_conditionData->_condition == VideoCondition::OBJECT) {
		auto objects = matchObject(
			screenshot, _conditionData->_objectCascade,
			_conditionData->_scaleFactor,
			_conditionData->_minNeighbors,
			{_conditionData->_minSizeX, _conditionData->_minSizeY},
			{_conditionData->_maxSizeX, _conditionData->_maxSizeY});
		if (objects.empty()) {
			resultIamge = screenshot;
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchSuccess"));
			resultIamge = markObjects(screenshot, objects);
		}
	}
	return resultIamge;
}
