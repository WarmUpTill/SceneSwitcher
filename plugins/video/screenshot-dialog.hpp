#pragma once
#include "screenshot-helper.hpp"
#include "parameter-wrappers.hpp"

#include <optional>
#include <obs.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QImage>
#include <QLabel>
#include <QRubberBand>
#include <QScrollArea>

namespace advss {

class ScreenshotDialog : public QDialog {
	Q_OBJECT

public:
	static std::optional<QImage> AskForScreenshot(const VideoInput &,
						      const AreaParameters &);

private:
	ScreenshotDialog(obs_source_t *source, const AreaParameters &area);

	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);

	QScrollArea *_scrollArea;
	QLabel *_imageLabel;

	QPoint _origin;
	QRubberBand *_rubberBand = nullptr;

	QDialogButtonBox *_buttonBox;

	QImage _result;
	Screenshot _screenshot;
};

} // namespace advss
