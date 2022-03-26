#include "preview-dialog.hpp"
#include "macro-condition-video.hpp"
#include "opencv-helpers.hpp"
#include "utility.hpp"

#include <condition_variable>

PreviewDialog::PreviewDialog(QWidget *parent,
			     MacroConditionVideo *conditionData,
			     std::mutex *mutex)
	: QDialog(parent),
	  _conditionData(conditionData),
	  _scrollArea(new QScrollArea),
	  _imageLabel(new QLabel(this)),
	  _rubberBand(new QRubberBand(QRubberBand::Rectangle, this)),
	  _mtx(mutex)
{
	setWindowTitle("Advanced Scene Switcher");
	_statusLabel = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.showMatch.loading"));
	resize(640, 480);

	_scrollArea->setBackgroundRole(QPalette::Dark);
	_scrollArea->setWidget(_imageLabel);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_statusLabel);
	layout->addWidget(_scrollArea);
	setLayout(layout);

	// This is a workaround to handle random segfaults triggered when using:
	// QMetaObject::invokeMethod(this, "RedrawImage", Qt::QueuedConnection,
	//			     Q_ARG(QImage, image));
	// from within CheckForMatchLoop().
	// Even using BlockingQueuedConnection causes deadlocks
	_timer.setInterval(500);
	QWidget::connect(&_timer, &QTimer::timeout, this,
			 &PreviewDialog::Resize);
	_timer.start();
}

void PreviewDialog::mousePressEvent(QMouseEvent *event)
{
	_selectingArea = true;
	if (_type == Type::SHOW_MATCH) {
		return;
	}
	_origin = event->pos();
	_rubberBand->setGeometry(QRect(_origin, QSize()));
	_rubberBand->show();
}

void PreviewDialog::mouseMoveEvent(QMouseEvent *event)
{
	_rubberBand->setGeometry(QRect(_origin, event->pos()).normalized());
}

void PreviewDialog::mouseReleaseEvent(QMouseEvent *)
{
	auto selectionStart =
		_rubberBand->mapToGlobal(_rubberBand->rect().topLeft());
	QRect selectionArea(selectionStart, _rubberBand->size());

	auto imageStart =
		_imageLabel->mapToGlobal(_imageLabel->rect().topLeft());
	QRect imageArea(imageStart, _imageLabel->size());

	auto intersected = imageArea.intersected(selectionArea);
	QRect checksize(QPoint(intersected.topLeft() - imageStart),
			intersected.size());
	if (checksize.x() >= 0 && checksize.y() >= 0 && checksize.width() > 0 &&
	    checksize.height() > 0) {
		emit SelectionAreaChanged(checksize);
	}
	_selectingArea = false;
}

PreviewDialog::~PreviewDialog()
{
	_stop = true;
	if (_thread.joinable()) {
		_thread.join();
	}
}

void PreviewDialog::ShowMatch()
{
	Start();
	_rubberBand->hide();
	_type = Type::SHOW_MATCH;
}

void PreviewDialog::SelectArea()
{
	_selectingArea = false;
	Start();
	_type = Type::SELECT_AREA;
	DrawFrame();
	_statusLabel->setText(obs_module_text(
		"AdvSceneSwitcher.condition.video.selectArea.status"));
}

void PreviewDialog::Resize()
{
	_imageLabel->adjustSize();
	if (_type == Type::SELECT_AREA && !_selectingArea) {
		DrawFrame();
	}
}

void PreviewDialog::Start()
{
	if (_thread.joinable()) {
		return;
	}
	if (!_conditionData) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		return;
	}
	_thread = std::thread(&PreviewDialog::CheckForMatchLoop, this);
}

void PreviewDialog::CheckForMatchLoop()
{
	std::condition_variable cv;
	while (!_stop) {
		std::unique_lock<std::mutex> lock(*_mtx);
		auto source = obs_weak_source_get_source(
			_conditionData->_videoSource);
		ScreenshotHelper screenshot(source);
		obs_source_release(source);
		cv.wait_for(lock, std::chrono::seconds(1));
		if (_stop) {
			return;
		}
		if (isHidden()) {
			continue;
		}
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

		if (_type == Type::SHOW_MATCH) {
			if (_conditionData->_checkAreaEnable) {
				screenshot.image = screenshot.image.copy(
					_conditionData->_checkArea.x,
					_conditionData->_checkArea.y,
					_conditionData->_checkArea.width,
					_conditionData->_checkArea.height);
			}
			MarkMatch(screenshot.image);
		}
		_imageLabel->setPixmap(QPixmap::fromImage(screenshot.image));
	}
}

void markPatterns(cv::Mat &matchResult, QImage &image, QImage &pattern)
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
}

void markObjects(QImage &image, std::vector<cv::Rect> &objects)
{
	auto frame = QImageToMat(image);
	for (size_t i = 0; i < objects.size(); i++) {
		rectangle(frame, cv::Point(objects[i].x, objects[i].y),
			  cv::Point(objects[i].x + objects[i].width,
				    objects[i].y + objects[i].height),
			  cv::Scalar(255, 0, 0, 255), 2, 8, 0);
	}
}

void PreviewDialog::MarkMatch(QImage &screenshot)
{
	if (_conditionData->_condition == VideoCondition::PATTERN) {
		cv::Mat result;
		QImage pattern = _conditionData->GetMatchImage();
		matchPattern(screenshot, pattern,
			     _conditionData->_patternThreshold, result,
			     _conditionData->_useAlphaAsMask);
		if (countNonZero(result) == 0) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchSuccess"));
			markPatterns(result, screenshot, pattern);
		}
	} else if (_conditionData->_condition == VideoCondition::OBJECT) {
		auto objects = matchObject(screenshot,
					   _conditionData->_objectCascade,
					   _conditionData->_scaleFactor,
					   _conditionData->_minNeighbors,
					   _conditionData->_minSize.CV(),
					   _conditionData->_maxSize.CV());
		if (objects.empty()) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchSuccess"));
			markObjects(screenshot, objects);
		}
	}
}

void PreviewDialog::DrawFrame()
{
	if (!_conditionData) {
		return;
	}
	auto imageStart =
		_imageLabel->mapToGlobal(_imageLabel->rect().topLeft());
	auto windowStart = mapToGlobal(rect().topLeft());
	_rubberBand->resize(_conditionData->_checkArea.width,
			    _conditionData->_checkArea.height);
	_rubberBand->move(_conditionData->_checkArea.x +
				  (imageStart.x() - windowStart.x()),
			  _conditionData->_checkArea.y +
				  (imageStart.y() - windowStart.y()));
	_rubberBand->show();
}
