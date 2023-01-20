#include "preview-dialog.hpp"

#include "opencv-helpers.hpp"
#include "utility.hpp"

#include <screenshot-helper.hpp>

PreviewDialog::PreviewDialog(QWidget *parent, int delay)
	: QDialog(parent),
	  _scrollArea(new QScrollArea),
	  _imageLabel(new QLabel(this)),
	  _rubberBand(new QRubberBand(QRubberBand::Rectangle, this)),
	  _delay(delay)
{
	setWindowTitle("Advanced Scene Switcher");
	setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint |
		       Qt::WindowCloseButtonHint);

	_statusLabel = new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.showMatch.loading"));
	resize(parent->window()->size());

	// Center image
	auto wrapper = new QWidget();
	auto wrapperHLayout = new QHBoxLayout();
	wrapperHLayout->addStretch();
	wrapperHLayout->addWidget(_imageLabel);
	wrapperHLayout->addStretch();
	auto wrapperVLayout = new QVBoxLayout();
	wrapperVLayout->addStretch();
	wrapperVLayout->addLayout(wrapperHLayout);
	wrapperVLayout->addStretch();
	wrapper->setLayout(wrapperVLayout);

	_scrollArea->setBackgroundRole(QPalette::Dark);
	_scrollArea->setWidget(wrapper);
	_scrollArea->setWidgetResizable(true);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(_statusLabel);
	layout->addWidget(_scrollArea);
	setLayout(layout);

	// This is a workaround to handle random segfaults triggered when using:
	// QMetaObject::invokeMethod(this, "RedrawImage", Qt::QueuedConnection,
	//			     Q_ARG(QImage, image));
	// from within CheckForMatchLoop().
	// Even using BlockingQueuedConnection causes deadlocks
	_timer.setInterval(300);
	QWidget::connect(&_timer, &QTimer::timeout, this,
			 &PreviewDialog::ResizeImageLabel);
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
	if (_type == Type::SHOW_MATCH) {
		return;
	}
	_rubberBand->setGeometry(QRect(_origin, event->pos()).normalized());
}

void PreviewDialog::mouseReleaseEvent(QMouseEvent *)
{
	if (_type == Type::SHOW_MATCH) {
		return;
	}
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
	Stop();
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

void PreviewDialog::Stop()
{
	_stop = true;
	_cv.notify_all();
	if (_thread.joinable()) {
		_thread.join();
	}
}

void PreviewDialog::closeEvent(QCloseEvent *event)
{
	Stop();
}

void PreviewDialog::PatternMatchParamtersChanged(
	const PatternMatchParameters &params)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_patternMatchParams = params;
	_patternImageData = createPatternData(_patternMatchParams.image);
}

void PreviewDialog::ObjDetectParamtersChanged(const ObjDetectParamerts &params)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_objDetectParams = params;
}

void PreviewDialog::VideoSelectionChanged(const VideoInput &video)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_video = video;
}

void PreviewDialog::AreaParamtersChanged(const AreaParamters &params)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_areaParams = params;
}

void PreviewDialog::ConditionChanged(int cond)
{
	Stop();
	close();

	std::unique_lock<std::mutex> lock(_mtx);
	_condition = static_cast<VideoCondition>(cond);
}

void PreviewDialog::ResizeImageLabel()
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
	if (!_video.ValidSelection()) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		close();
		return;
	}
	_stop = false;
	_thread = std::thread(&PreviewDialog::CheckForMatchLoop, this);
}

void PreviewDialog::CheckForMatchLoop()
{
	while (!_stop) {
		std::unique_lock<std::mutex> lock(_mtx);
		auto source = obs_weak_source_get_source(_video.GetVideo());
		ScreenshotHelper screenshot(source);
		obs_source_release(source);
		_cv.wait_for(lock, std::chrono::milliseconds(_delay));
		if (_stop || isHidden()) {
			return;
		}
		if (!screenshot.done || !_video.ValidSelection()) {
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
			if (_areaParams.enable) {
				screenshot.image = screenshot.image.copy(
					_areaParams.area.x, _areaParams.area.y,
					_areaParams.area.width,
					_areaParams.area.height);
			}
			MarkMatch(screenshot.image);
		}
		_imageLabel->setPixmap(QPixmap::fromImage(screenshot.image));
	}
}

void markPatterns(cv::Mat &matchResult, QImage &image, cv::Mat &pattern)
{
	auto matchImg = QImageToMat(image);
	for (int row = 0; row < matchResult.rows - 1; row++) {
		for (int col = 0; col < matchResult.cols - 1; col++) {
			if (matchResult.at<float>(row, col) != 0.0) {
				rectangle(matchImg, {col, row},
					  cv::Point(col + pattern.cols,
						    row + pattern.rows),
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
	if (_condition == VideoCondition::PATTERN) {
		cv::Mat result;
		matchPattern(screenshot, _patternImageData,
			     _patternMatchParams.threshold, result,
			     _patternMatchParams.useAlphaAsMask);
		if (countNonZero(result) == 0) {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchFail"));
		} else {
			_statusLabel->setText(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchSuccess"));
			markPatterns(result, screenshot,
				     _patternImageData.rgbaPattern);
		}
	} else if (_condition == VideoCondition::OBJECT) {
		auto objects = matchObject(screenshot, _objDetectParams.cascade,
					   _objDetectParams.scaleFactor,
					   _objDetectParams.minNeighbors,
					   _objDetectParams.minSize.CV(),
					   _objDetectParams.maxSize.CV());
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
	if (!_video.ValidSelection()) {
		return;
	}
	auto imageStart =
		_imageLabel->mapToGlobal(_imageLabel->rect().topLeft());
	auto windowStart = mapToGlobal(rect().topLeft());
	_rubberBand->resize(_areaParams.area.width, _areaParams.area.height);
	_rubberBand->move(
		_areaParams.area.x + (imageStart.x() - windowStart.x()),
		_areaParams.area.y + (imageStart.y() - windowStart.y()));
	_rubberBand->show();
}
