#include "screenshot-dialog.hpp"
#include "obs-module-helper.hpp"
#include "ui-helpers.hpp"

#include <QLayout>
#include <QMouseEvent>

namespace advss {

ScreenshotDialog::ScreenshotDialog(obs_source_t *source,
				   const AreaParameters &area)
	: QDialog(GetSettingsWindow()),
	  _scrollArea(new QScrollArea),
	  _imageLabel(new QLabel(this)),
	  _rubberBand(new QRubberBand(QRubberBand::Rectangle, this)),
	  _buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok |
					  QDialogButtonBox::Cancel)),
	  _screenshot(source,
		      area.enable ? QRect(area.area.x, area.area.y,
					  area.area.width, area.area.height)
				  : QRect(),
		      true)
{
	setWindowTitle(obs_module_text("AdvSceneSwitcher.windowTitle"));
	setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint |
		       Qt::WindowCloseButtonHint);

	QWidget::connect(_buttonBox, &QDialogButtonBox::accepted, this,
			 &QDialog::accept);
	QWidget::connect(_buttonBox, &QDialogButtonBox::rejected, this,
			 &QDialog::reject);

	_imageLabel->setPixmap(QPixmap::fromImage(_screenshot.GetImage()));
	_imageLabel->adjustSize();
	_imageLabel->updateGeometry();

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
	layout->addWidget(new QLabel(obs_module_text(
		"AdvSceneSwitcher.condition.video.screenshot.selectArea")));
	layout->addWidget(_scrollArea);
	layout->addWidget(_buttonBox);
	setLayout(layout);

	_result = _screenshot.GetImage();

	if (!_screenshot.IsDone() || _screenshot.GetImage().isNull()) {
		DisplayMessage(obs_module_text(
			"AdvSceneSwitcher.condition.video.screenshotFail"));
		close();
	}
}

void ScreenshotDialog::mousePressEvent(QMouseEvent *event)
{
	_origin = event->pos();
	_rubberBand->setGeometry(QRect(_origin, QSize()));
	_rubberBand->show();
}

void ScreenshotDialog::mouseMoveEvent(QMouseEvent *event)
{
	_rubberBand->setGeometry(QRect(_origin, event->pos()).normalized());
}

void ScreenshotDialog::mouseReleaseEvent(QMouseEvent *)
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
		_result = _screenshot.GetImage().copy(checksize.x(),
						      checksize.y(),
						      checksize.width(),
						      checksize.height());
	}
}

std::optional<QImage>
ScreenshotDialog::AskForScreenshot(const VideoInput &input,
				   const AreaParameters &area)
{
	if (!input.ValidSelection()) {
		return {};
	}

	auto source = OBSGetStrongRef(input.GetVideo());
	ScreenshotDialog dialog(source, area);
	if (dialog.exec() != DialogCode::Accepted) {
		return {};
	}

	return dialog._result;
}

} // namespace advss
