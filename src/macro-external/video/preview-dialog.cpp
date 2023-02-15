#include "preview-dialog.hpp"

#include "opencv-helpers.hpp"
#include "utility.hpp"

#include <screenshot-helper.hpp>

PreviewDialog::PreviewDialog(QWidget *parent)
	: QDialog(parent),
	  _scrollArea(new QScrollArea),
	  _imageLabel(new QLabel(this)),
	  _rubberBand(new QRubberBand(QRubberBand::Rectangle, this))
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
}

void PreviewDialog::mousePressEvent(QMouseEvent *event)
{
	_selectingArea = true;
	if (_type == PreviewType::SHOW_MATCH) {
		return;
	}
	_origin = event->pos();
	_rubberBand->setGeometry(QRect(_origin, QSize()));
	_rubberBand->show();
}

void PreviewDialog::mouseMoveEvent(QMouseEvent *event)
{
	if (_type == PreviewType::SHOW_MATCH) {
		return;
	}
	_rubberBand->setGeometry(QRect(_origin, event->pos()).normalized());
}

void PreviewDialog::mouseReleaseEvent(QMouseEvent *)
{
	if (_type == PreviewType::SHOW_MATCH) {
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
	_type = PreviewType::SHOW_MATCH;
	_rubberBand->hide();
	Start();
}

void PreviewDialog::SelectArea()
{
	_selectingArea = false;
	_type = PreviewType::SELECT_AREA;
	Start();
	DrawFrame();
	_statusLabel->setText(obs_module_text(
		"AdvSceneSwitcher.condition.video.selectArea.status"));
}

void PreviewDialog::Stop()
{
	_thread.quit();
	_thread.wait();
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

void PreviewDialog::ObjDetectParamtersChanged(const ObjDetectParameters &params)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_objDetectParams = params;
}

void PreviewDialog::OCRParamtersChanged(const OCRParameters &params)
{
	std::unique_lock<std::mutex> lock(_mtx);
	_ocrParams = params;
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

void PreviewDialog::UpdateStatus(const QString &status)
{
	_statusLabel->setText(status);
}

void PreviewDialog::UpdateImage(const QPixmap &image)
{
	_imageLabel->setPixmap(image);
	_imageLabel->adjustSize();
	if (_type == PreviewType::SELECT_AREA && !_selectingArea) {
		DrawFrame();
	}
	emit NeedImage(_video, _type, _patternMatchParams, _patternImageData,
		       _objDetectParams, _ocrParams, _areaParams, _condition);
}

void PreviewDialog::Start()
{
	if (!_video.ValidSelection()) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		close();
		return;
	}

	if (_thread.isRunning()) {
		return;
	}

	PreviewImage *worker = new PreviewImage();
	worker->moveToThread(&_thread);
	connect(&_thread, &QThread::finished, worker, &QObject::deleteLater);
	connect(worker, &PreviewImage::ImageReady, this,
		&PreviewDialog::UpdateImage);
	connect(worker, &PreviewImage::StatusUpdate, this,
		&PreviewDialog::UpdateStatus);
	connect(this, &PreviewDialog::NeedImage, worker,
		&PreviewImage::CreateImage);
	_thread.start();

	emit NeedImage(_video, _type, _patternMatchParams, _patternImageData,
		       _objDetectParams, _ocrParams, _areaParams, _condition);
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

void markPatterns(cv::Mat &matchResult, QImage &image, const cv::Mat &pattern)
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

void PreviewImage::CreateImage(const VideoInput &video, PreviewType type,
			       const PatternMatchParameters &patternMatchParams,
			       const PatternImageData &patternImageData,
			       ObjDetectParameters objDetectParams,
			       OCRParameters ocrParams,
			       const AreaParamters &areaParams,
			       VideoCondition condition)
{
	auto source = obs_weak_source_get_source(video.GetVideo());
	ScreenshotHelper screenshot(source, true);
	obs_source_release(source);

	if (!video.ValidSelection() || !screenshot.done) {
		emit StatusUpdate(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		emit ImageReady(QPixmap());
		return;
	}

	if (screenshot.image.width() == 0 || screenshot.image.height() == 0) {
		emit StatusUpdate(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotEmpty"));
		emit ImageReady(QPixmap());
		return;
	}

	if (type == PreviewType::SHOW_MATCH) {
		if (areaParams.enable) {
			screenshot.image = screenshot.image.copy(
				areaParams.area.x, areaParams.area.y,
				areaParams.area.width, areaParams.area.height);
		}
		// Will emit status label update
		MarkMatch(screenshot.image, patternMatchParams,
			  patternImageData, objDetectParams, ocrParams,
			  condition);
	} else {
		emit StatusUpdate("");
	}
	emit ImageReady(QPixmap::fromImage(screenshot.image));
}

void PreviewImage::MarkMatch(QImage &screenshot,
			     const PatternMatchParameters &patternMatchParams,
			     const PatternImageData &patternImageData,
			     ObjDetectParameters &objDetectParams,
			     const OCRParameters &ocrParams,
			     VideoCondition condition)
{
	if (condition == VideoCondition::PATTERN) {
		cv::Mat result;
		matchPattern(screenshot, patternImageData,
			     patternMatchParams.threshold, result,
			     patternMatchParams.useAlphaAsMask,
			     patternMatchParams.matchMode);
		if (countNonZero(result) == 0) {
			emit StatusUpdate(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchFail"));
		} else {
			emit StatusUpdate(obs_module_text(
				"AdvSceneSwitcher.condition.video.patternMatchSuccess"));
			markPatterns(result, screenshot,
				     patternImageData.rgbaPattern);
		}
	} else if (condition == VideoCondition::OBJECT) {
		auto objects = matchObject(screenshot, objDetectParams.cascade,
					   objDetectParams.scaleFactor,
					   objDetectParams.minNeighbors,
					   objDetectParams.minSize.CV(),
					   objDetectParams.maxSize.CV());
		if (objects.empty()) {
			emit StatusUpdate(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchFail"));
		} else {
			emit StatusUpdate(obs_module_text(
				"AdvSceneSwitcher.condition.video.objectMatchSuccess"));
			markObjects(screenshot, objects);
		}
	} else if (condition == VideoCondition::OCR) {
		auto text =
			runOCR(ocrParams.GetOCR(), screenshot, ocrParams.color);
		QString status(obs_module_text(
			"AdvSceneSwitcher.condition.video.ocrMatchSuccess"));
		emit StatusUpdate(status.arg(QString::fromStdString(text)));

		// TODO: show preprocessed image
	}
}
